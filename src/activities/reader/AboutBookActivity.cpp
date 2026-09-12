#include "AboutBookActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>

#include "MappedInputManager.h"
#include "components/UIScale.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

AboutBookActivity::AboutBookActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                     std::shared_ptr<Epub> epub)
    : Activity("AboutBook", renderer, mappedInput), UiAppHost(renderer), epub(std::move(epub)) {}

void AboutBookActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  app.setScreen(&AboutBookActivity::aboutBookScreen, this);

  if (epub) {
    snprintf(languageLine, sizeof(languageLine), tr(STR_BOOK_LANGUAGE_FORMAT), epub->getLanguage().c_str());
    snprintf(sectionsLine, sizeof(sectionsLine), tr(STR_BOOK_SECTIONS_FORMAT),
             static_cast<unsigned int>(epub->getSpineItemsCount()));

    // The thumbnail is generated once into the existing per-book SD cache.
    // Only its path remains in RAM; the decoded cover is streamed while drawing.
    coverAvailable = epub->generateThumbBmp(COVER_CACHE_HEIGHT);
    if (coverAvailable) coverPath = epub->getThumbBmpPath(COVER_CACHE_HEIGHT);

    const auto fonts = uiScaleSpec();
    renderer.ensureSdCardFontReady(fonts.titleFontId, epub->getTitle().c_str(), EpdFontFamily::BOLD);
    renderer.ensureSdCardFontReady(fonts.bodyFontId, epub->getAuthor().c_str(), EpdFontFamily::REGULAR);
    renderer.ensureSdCardFontReady(fonts.bodyFontId, epub->getDescription().c_str(), EpdFontFamily::REGULAR);
  }

  requestUpdate();
}

void AboutBookActivity::close() {
  ActivityResult result;
  result.isCancelled = true;
  setResult(std::move(result));
  finish();
}

void AboutBookActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    close();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::NavPrevious) && descriptionPage > 0) {
    descriptionPage--;
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::NavNext) &&
      descriptionPage + 1 < descriptionPageCount) {
    descriptionPage++;
    requestUpdate();
  }
}

void AboutBookActivity::aboutBookScreen(UiScreen& screen, void* user) {
  static_cast<AboutBookActivity*>(user)->buildScreen(screen);
}

size_t AboutBookActivity::descriptionPageEnd(const fui::DrawTarget& target, const char* text, const size_t textSize,
                                             const size_t start, const int16_t width, const uint8_t maxLines,
                                             fui::TextStyle style) {
  if (!text || start >= textSize || width <= 0 || maxLines == 0) return textSize;

  const size_t remaining = textSize - start;
  size_t low = 1;
  size_t high = remaining;
  size_t best = 0;
  style.maxLines = static_cast<uint8_t>(maxLines + 1);
  const int16_t lineHeight = std::max<int16_t>(1, target.lineHeight(style.font));

  while (low <= high) {
    const size_t midpoint = low + (high - low) / 2;
    size_t candidate = midpoint;
    while (candidate > 0 && candidate < remaining &&
           (static_cast<unsigned char>(text[start + candidate]) & 0xC0) == 0x80) {
      candidate--;
    }

    if (candidate == 0) {
      low = midpoint + 1;
      continue;
    }

    memcpy(descriptionPageText, text + start, candidate);
    descriptionPageText[candidate] = '\0';
    const int lines = fui::measureWrappedText(target, descriptionPageText, style, width).height / lineHeight;
    if (lines <= maxLines) {
      best = candidate;
      low = midpoint + 1;
    } else {
      if (midpoint == 0) break;
      high = midpoint - 1;
    }
  }

  if (best == 0) {
    best = 1;
    while (best < remaining && (static_cast<unsigned char>(text[start + best]) & 0xC0) == 0x80) best++;
  }

  if (start + best < textSize && !std::isspace(static_cast<unsigned char>(text[start + best]))) {
    size_t wordBoundary = best;
    while (wordBoundary > 0 &&
           !std::isspace(static_cast<unsigned char>(text[start + wordBoundary - 1]))) {
      wordBoundary--;
    }
    if (wordBoundary > 0) best = wordBoundary;
  }

  size_t end = start + best;
  while (end < textSize && std::isspace(static_cast<unsigned char>(text[end]))) end++;
  return end;
}

void AboutBookActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});

  const auto& theme = screen.theme();
  screen.insetContent(fui::Insets{theme.spaceMd, theme.spaceLg, theme.spaceMd, theme.spaceLg});
  const fui::Rect content = screen.body();
  const bool landscape = content.width > content.height;
  const int16_t coverHeight = std::min<int16_t>(COVER_CACHE_HEIGHT,
                                                static_cast<int16_t>(content.height * (landscape ? 58 : 42) / 100));
  const int16_t coverWidth = std::min<int16_t>(static_cast<int16_t>(coverHeight * 3 / 5),
                                               static_cast<int16_t>(content.width * 35 / 100));
  const int16_t gap = theme.spaceLg;
  const fui::Rect cover{content.x, content.y, coverWidth, coverHeight};
  coverRect = Rect{cover.x, cover.y, cover.width, cover.height};

  if (!coverAvailable) {
    screen.target().stroke(cover, fui::Paint::solid(fui::Color::Black), 1);
    fui::TextStyle emptyCover = theme.smallText;
    emptyCover.align = fui::TextAlign::Center;
    emptyCover.maxLines = 2;
    screen.target().text(cover.inset(fui::Insets{theme.spaceMd, theme.spaceMd, theme.spaceMd, theme.spaceMd}),
                         tr(STR_NO_COVER), emptyCover);
  }

  const int16_t infoX = static_cast<int16_t>(cover.right() + gap);
  const int16_t infoWidth = std::max<int16_t>(0, static_cast<int16_t>(content.right() - infoX));
  int16_t infoY = content.y;

  fui::TextStyle titleStyle = theme.titleText;
  titleStyle.bold = true;
  titleStyle.maxLines = 3;
  const char* title = epub && !epub->getTitle().empty() ? epub->getTitle().c_str() : tr(STR_UNNAMED);
  const int16_t measuredTitleHeight =
      fui::measureWrappedText(screen.target(), title, titleStyle, infoWidth).height;
  const int16_t titleHeight = std::min<int16_t>(coverHeight, measuredTitleHeight);
  screen.target().text(fui::Rect{infoX, infoY, infoWidth, titleHeight}, title, titleStyle);
  infoY = static_cast<int16_t>(infoY + titleHeight + theme.spaceMd);

  fui::TextStyle bodyStyle = theme.bodyText;
  bodyStyle.maxLines = 2;
  if (epub && !epub->getAuthor().empty() && infoY < cover.bottom()) {
    const int16_t authorHeight =
        fui::measureWrappedText(screen.target(), epub->getAuthor().c_str(), bodyStyle, infoWidth).height;
    const int16_t height =
        std::min<int16_t>(authorHeight, static_cast<int16_t>(cover.bottom() - infoY));
    screen.target().text(fui::Rect{infoX, infoY, infoWidth, height}, epub->getAuthor().c_str(), bodyStyle);
    infoY = static_cast<int16_t>(infoY + height + theme.spaceMd);
  }

  fui::TextStyle detailStyle = theme.smallText;
  detailStyle.maxLines = 1;
  if (epub && !epub->getLanguage().empty() && infoY < cover.bottom()) {
    const int16_t height = screen.target().lineHeight(detailStyle.font);
    screen.target().text(fui::Rect{infoX, infoY, infoWidth, height}, languageLine, detailStyle);
    infoY = static_cast<int16_t>(infoY + height + theme.spaceSm);
  }
  if (epub && epub->getSpineItemsCount() > 0 && infoY < cover.bottom()) {
    const int16_t height = screen.target().lineHeight(detailStyle.font);
    screen.target().text(fui::Rect{infoX, infoY, infoWidth, height}, sectionsLine, detailStyle);
  }

  const int16_t descriptionY = static_cast<int16_t>(cover.bottom() + theme.spaceLg);
  const int16_t labelHeight = screen.target().lineHeight(theme.smallText.font);
  if (descriptionY + labelHeight >= content.bottom()) return;

  fui::TextStyle labelStyle = theme.smallText;
  labelStyle.bold = true;
  screen.target().text(fui::Rect{content.x, descriptionY, content.width, labelHeight}, tr(STR_DESCRIPTION),
                       labelStyle);

  const int16_t bodyY = static_cast<int16_t>(descriptionY + labelHeight + theme.spaceSm);
  const int16_t bodyHeight = static_cast<int16_t>(content.bottom() - bodyY);
  if (bodyHeight <= 0) return;
  const int lineHeight = std::max<int>(1, screen.target().lineHeight(bodyStyle.font));
  // Keep one extra SDK layout line available as an overflow probe while
  // finding each page boundary. FreeInkUI caps wrapped blocks at 16 lines.
  bodyStyle.maxLines = static_cast<uint8_t>(std::clamp<int>(bodyHeight / lineHeight, 1, 15));
  const char* description = epub && !epub->getDescription().empty() ? epub->getDescription().c_str()
                                                                      : tr(STR_NO_DESCRIPTION);
  const size_t descriptionSize = strlen(description);
  size_t cursor = 0;
  size_t selectedStart = 0;
  size_t selectedEnd = descriptionSize;
  size_t lastStart = 0;
  descriptionPageCount = 0;
  while (cursor < descriptionSize) {
    const size_t end = descriptionPageEnd(screen.target(), description, descriptionSize, cursor, content.width,
                                          bodyStyle.maxLines, bodyStyle);
    if (descriptionPageCount == descriptionPage) {
      selectedStart = cursor;
      selectedEnd = end;
    }
    lastStart = cursor;
    descriptionPageCount++;
    if (end <= cursor) break;
    cursor = end;
  }
  descriptionPageCount = std::max(1, descriptionPageCount);
  if (descriptionPage >= descriptionPageCount) {
    descriptionPage = descriptionPageCount - 1;
    selectedStart = lastStart;
    selectedEnd = descriptionPageEnd(screen.target(), description, descriptionSize, selectedStart, content.width,
                                     bodyStyle.maxLines, bodyStyle);
  }

  size_t renderEnd = selectedEnd;
  while (renderEnd > selectedStart && std::isspace(static_cast<unsigned char>(description[renderEnd - 1]))) {
    renderEnd--;
  }
  const size_t pageBytes = std::min(renderEnd - selectedStart, DESCRIPTION_BUFFER_SIZE - 1);
  memcpy(descriptionPageText, description + selectedStart, pageBytes);
  descriptionPageText[pageBytes] = '\0';

  if (descriptionPageCount > 1) {
    snprintf(descriptionPageLine, sizeof(descriptionPageLine), tr(STR_PAGE_COUNT_FORMAT),
             static_cast<unsigned int>(descriptionPage + 1), static_cast<unsigned int>(descriptionPageCount));
    fui::TextStyle pageStyle = labelStyle;
    pageStyle.align = fui::TextAlign::Right;
    screen.target().text(fui::Rect{content.x, descriptionY, content.width, labelHeight}, descriptionPageLine,
                         pageStyle);
  }

  const int16_t descriptionHeight =
      std::min<int16_t>(bodyHeight,
                        fui::measureWrappedText(screen.target(), descriptionPageText, bodyStyle, content.width).height);
  screen.target().text(fui::Rect{content.x, bodyY, content.width, descriptionHeight}, descriptionPageText, bodyStyle);
}

void AboutBookActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  GUI.drawHeader(renderer, Rect{safe.x, safe.y + metrics.topPadding, safe.width, metrics.headerHeight},
                 tr(STR_ABOUT_THIS_BOOK));

  renderUi();
  if (coverAvailable && !GUI.drawBookCover(renderer, coverRect, coverPath.c_str())) {
    coverAvailable = false;
    requestUpdate();
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", descriptionPage > 0 ? tr(STR_DIR_UP) : "",
                                            descriptionPage + 1 < descriptionPageCount ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
