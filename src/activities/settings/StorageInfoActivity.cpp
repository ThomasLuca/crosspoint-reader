#include "StorageInfoActivity.h"

#include <HalStorage.h>
#include <I18n.h>

#include <cstdio>

#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
constexpr uint64_t BYTES_PER_GB = 1000ULL * 1000ULL * 1000ULL;
constexpr uint64_t BYTES_PER_MB = 1000ULL * 1000ULL;
}

StorageInfoActivity::StorageInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("StorageInfo", renderer, mappedInput) {}

const char* StorageInfoActivity::headerTitle() const { return tr(STR_STORAGE_INFORMATION); }

void StorageInfoActivity::onEnter() {
  UiListActivity::onEnter();

  const StrId labels[ITEM_COUNT] = {StrId::STR_STORAGE_USED, StrId::STR_STORAGE_FREE, StrId::STR_STORAGE_TOTAL};
  uint64_t totalBytes = 0;
  uint64_t usedBytes = 0;
  const bool available = Storage.storageUsage(totalBytes, usedBytes);

  for (int i = 0; i < ITEM_COUNT; ++i) {
    rowItems[i].label = I18N.get(labels[i]);
    rowItems[i].actionValue = static_cast<int16_t>(i);
  }

  if (!available || usedBytes > totalBytes) {
    for (int i = 0; i < ITEM_COUNT; ++i) {
      snprintf(rowValues[i], sizeof(rowValues[i]), "%s", tr(STR_STORAGE_UNAVAILABLE));
      rowItems[i].value = rowValues[i];
    }
    return;
  }

  const uint64_t freeBytes = totalBytes - usedBytes;

  formatCapacity(rowValues[0], sizeof(rowValues[0]), usedBytes);
  formatCapacity(rowValues[1], sizeof(rowValues[1]), freeBytes);
  formatCapacity(rowValues[2], sizeof(rowValues[2]), totalBytes);
  for (int i = 0; i < ITEM_COUNT; ++i) {
    rowItems[i].value = rowValues[i];
  }
}

void StorageInfoActivity::formatCapacity(char* buffer, const size_t bufferSize, const uint64_t bytes) const {
  const uint64_t unit = bytes >= BYTES_PER_GB ? BYTES_PER_GB : BYTES_PER_MB;
  const char* suffix = bytes >= BYTES_PER_GB ? "GB" : "MB";
  const uint64_t tenths = (bytes * 10ULL + unit / 2ULL) / unit;
  snprintf(buffer, bufferSize, "%llu.%llu %s", static_cast<unsigned long long>(tenths / 10ULL),
           static_cast<unsigned long long>(tenths % 10ULL), suffix);
}

void StorageInfoActivity::activateIndex(const int index) {
  (void)index;
  app.clearTapFlash();
}

void StorageInfoActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems;
  props.count = ITEM_COUNT;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
