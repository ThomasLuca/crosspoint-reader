#pragma once

#include <Epub.h>

#include <cstddef>
#include <memory>
#include <string>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "components/themes/BaseTheme.h"

class AboutBookActivity final : public Activity, private UiAppHost {
 public:
  AboutBookActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::shared_ptr<Epub> epub);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }

 private:
  static constexpr int COVER_CACHE_HEIGHT = 240;
  static constexpr size_t DESCRIPTION_BUFFER_SIZE = 2049;

  static void aboutBookScreen(UiScreen& screen, void* user);
  void buildScreen(UiScreen& screen);
  size_t descriptionPageEnd(const freeink::ui::DrawTarget& target, const char* text, size_t textSize,
                            size_t start, int16_t width, uint8_t maxLines, freeink::ui::TextStyle style);
  void close();

  std::shared_ptr<Epub> epub;
  std::string coverPath;
  Rect coverRect{};
  bool coverAvailable = false;
  int descriptionPage = 0;
  int descriptionPageCount = 1;
  char languageLine[64]{};
  char sectionsLine[64]{};
  char descriptionPageText[DESCRIPTION_BUFFER_SIZE]{};
  char descriptionPageLine[24]{};
};
