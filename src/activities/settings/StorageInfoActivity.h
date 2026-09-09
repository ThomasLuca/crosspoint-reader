#pragma once

#include <cstdint>

#include "activities/UiListActivity.h"

class StorageInfoActivity final : public UiListActivity {
 public:
  explicit StorageInfoActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  static constexpr int ITEM_COUNT = 3;

  int listCount() const override { return ITEM_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  void formatCapacity(char* buffer, size_t bufferSize, uint64_t bytes) const;

  freeink::ui::ListItem rowItems[ITEM_COUNT]{};
  char rowValues[ITEM_COUNT][32]{};
};
