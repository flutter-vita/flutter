// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "txt/platform.h"

#include <cctype>
#include <mutex>
#include <unordered_map>
#include <utility>

#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontArguments.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontMgr_directory.h"

namespace txt {

namespace {

// Where the port keeps its fonts, and why it is not flutter_assets.
//
// `app0:` is the read-only mount of the installed .vpk, so anything here ships
// with the application and is found by path -- there is no fontconfig on this
// platform and no system font store to enumerate.
//
// It is deliberately a directory of its own rather than
// `app0:/flutter_assets/fonts`. That directory belongs to the asset bundle:
// `flutter build bundle` owns its contents, and it holds icon fonts like
// MaterialIcons that are meant to be looked up by asset key, not offered as
// system families. Scanning it would make every icon font a candidate for text
// fallback, which produces text rendered in icons -- a failure that looks like
// a font bug and is a configuration one.
//
// That last point is load-bearing now that this file implements character
// fallback below: everything in this directory is a candidate for every
// codepoint, so what goes in it is a deliberate choice, not a convenience.
constexpr char kFontDirectory[] = "app0:/fonts";

// Character fallback, which the directory font manager does not do.
//
// `SkFontMgr_Custom::onMatchFamilyStyleCharacter` returns nullptr -- see
// skia/src/ports/SkFontMgr_custom.cpp. That is the hook Skia calls when the
// family in hand has no glyph for a codepoint, and returning nullptr means the
// answer is `.notdef` and the user sees tofu. On Android and the desktops the
// platform font manager answers it, which is why nobody notices that Roboto
// alone covers 896 codepoints.
//
// Found on 2026-08-28 chasing emoji in keklist, which rendered as boxes. The
// first fix was app-side -- bundle an emoji font, name it in
// `fontFamilyFallback` -- and it works, but it only works for apps whose source
// you can edit. Third-party apps run here unmodified, which is the entire point
// of the corpus, and no unmodified Flutter app names a Vita-specific family.
//
// So this wraps the directory manager and answers the hook by scanning what the
// directory holds for a face that actually has the glyph. Two properties worth
// stating, because both are deliberate:
//
//   * The result is cached per codepoint, misses included. The directory is on
//     a read-only mount and cannot change while the process lives, so a miss
//     stays a miss and rescanning would be pure cost. Text hits this path for
//     every unmatched run, not once.
//
//   * The cache key is the codepoint alone, ignoring the requested style. A
//     bold emoji therefore resolves to the regular emoji face. That is correct
//     for every font this port is likely to carry -- emoji and symbol fonts
//     ship one style -- and the alternative is a cache keyed on a struct for
//     a distinction nothing can currently observe.
//
// `bcp47` is ignored. Skia's emoji path passes `und-Zsye` as a hint and the
// general path passes the locale; with a directory this small, coverage is a
// better discriminator than either, and honouring the hint would only matter
// once two fonts both claim a codepoint.
// Generic family names, mapped to what app0:/fonts actually holds.
//
// SkFontMgr_Custom matches on the family name stored inside the font file, so
// "monospace" matches nothing, onMatchFamilyStyle returns null, and
// skparagraph falls back to the default family. Everything that asked for a
// fixed-pitch face got Roboto, which is proportional: a register dump and a
// byte column do not line up and nothing says why. The receiver alone asks for
// "monospace" in six places, and it is the panel you read crash reports in.
//
// The wider case is the whole point of the port. An application we did not
// write names the families its author's platform has -- "sans-serif",
// "Courier New", "Menlo", "Helvetica" -- and every one of them silently became
// Roboto.
//
// The asked-for name is tried first, so an application that bundles its own
// "Roboto Mono" wins over this table.
struct FamilyAlias {
  const char* asked;
  const char* shipped;
};

constexpr FamilyAlias kFamilyAliases[] = {
    {"monospace", "Roboto Mono"},
    {"mono", "Roboto Mono"},
    {"ui-monospace", "Roboto Mono"},
    {"courier", "Roboto Mono"},
    {"courier new", "Roboto Mono"},
    {"menlo", "Roboto Mono"},
    {"monaco", "Roboto Mono"},
    {"consolas", "Roboto Mono"},
    {"sf mono", "Roboto Mono"},
    {"dejavu sans mono", "Roboto Mono"},
    {"liberation mono", "Roboto Mono"},
    {"sans-serif", "Roboto"},
    {"sans", "Roboto"},
    {"system-ui", "Roboto"},
    {"arial", "Roboto"},
    {"helvetica", "Roboto"},
    {"helvetica neue", "Roboto"},
    {"segoe ui", "Roboto"},
    {"verdana", "Roboto"},
    {"tahoma", "Roboto"},
    // No serif face ships. Roboto is the wrong shape for these and is still
    // the honest answer: it is what they resolve to today, by accident,
    // through the default family. Naming it here costs nothing and makes the
    // day a serif face ships a one-line change.
    {"serif", "Roboto"},
    {"times", "Roboto"},
    {"times new roman", "Roboto"},
    {"georgia", "Roboto"},
};

bool FamilyNameEquals(const char* a, const char* b) {
  while (*a != '\0' && *b != '\0') {
    if (std::tolower(static_cast<unsigned char>(*a)) !=
        std::tolower(static_cast<unsigned char>(*b))) {
      return false;
    }
    a++;
    b++;
  }
  return *a == *b;
}

const char* AliasFor(const char* familyName) {
  if (familyName == nullptr) {
    return nullptr;
  }
  for (const FamilyAlias& alias : kFamilyAliases) {
    if (FamilyNameEquals(familyName, alias.asked)) {
      return alias.shipped;
    }
  }
  return nullptr;
}

class VitaFallbackFontMgr final : public SkFontMgr {
 public:
  explicit VitaFallbackFontMgr(sk_sp<SkFontMgr> delegate)
      : delegate_(std::move(delegate)) {}

 protected:
  int onCountFamilies() const override { return delegate_->countFamilies(); }

  void onGetFamilyName(int index, SkString* familyName) const override {
    delegate_->getFamilyName(index, familyName);
  }

  sk_sp<SkFontStyleSet> onCreateStyleSet(int index) const override {
    return delegate_->createStyleSet(index);
  }

  sk_sp<SkFontStyleSet> onMatchFamily(const char familyName[]) const override {
    // matchFamily is wrapped in emptyOnNull upstream, so an unknown family is
    // an empty set rather than null. Count it, do not test the pointer.
    sk_sp<SkFontStyleSet> set = delegate_->matchFamily(familyName);
    if (set != nullptr && set->count() > 0) {
      return set;
    }
    if (const char* alias = AliasFor(familyName)) {
      return delegate_->matchFamily(alias);
    }
    return set;
  }

  sk_sp<SkTypeface> onMatchFamilyStyle(const char familyName[],
                                       const SkFontStyle& style) const override {
    if (sk_sp<SkTypeface> hit = delegate_->matchFamilyStyle(familyName, style)) {
      return hit;
    }
    if (const char* alias = AliasFor(familyName)) {
      return delegate_->matchFamilyStyle(alias, style);
    }
    return nullptr;
  }

  sk_sp<SkTypeface> onMatchFamilyStyleCharacter(const char familyName[],
                                                const SkFontStyle& style,
                                                const char* bcp47[],
                                                int bcp47Count,
                                                SkUnichar character) const override {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto cached = cache_.find(character);
    if (cached != cache_.end()) {
      return cached->second;
    }

    sk_sp<SkTypeface> found;
    const int families = delegate_->countFamilies();
    for (int i = 0; i < families && found == nullptr; i++) {
      sk_sp<SkFontStyleSet> set = delegate_->createStyleSet(i);
      if (set == nullptr) {
        continue;
      }
      sk_sp<SkTypeface> face = set->matchStyle(style);
      // Glyph 0 is `.notdef` by definition, so this is the coverage test: the
      // cmap either maps the codepoint to a real glyph or it does not.
      if (face != nullptr && face->unicharToGlyph(character) != 0) {
        found = std::move(face);
      }
    }

    cache_[character] = found;
    return found;
  }

  sk_sp<SkTypeface> onMakeFromData(sk_sp<SkData> data, int ttcIndex) const override {
    return delegate_->makeFromData(std::move(data), ttcIndex);
  }

  sk_sp<SkTypeface> onMakeFromStreamIndex(std::unique_ptr<SkStreamAsset> stream,
                                          int ttcIndex) const override {
    return delegate_->makeFromStream(std::move(stream), ttcIndex);
  }

  sk_sp<SkTypeface> onMakeFromStreamArgs(std::unique_ptr<SkStreamAsset> stream,
                                         const SkFontArguments& args) const override {
    return delegate_->makeFromStream(std::move(stream), args);
  }

  sk_sp<SkTypeface> onMakeFromFile(const char path[], int ttcIndex) const override {
    return delegate_->makeFromFile(path, ttcIndex);
  }

  sk_sp<SkTypeface> onLegacyMakeTypeface(const char familyName[],
                                         SkFontStyle style) const override {
    if (sk_sp<SkTypeface> hit =
            delegate_->legacyMakeTypeface(familyName, style)) {
      return hit;
    }
    if (const char* alias = AliasFor(familyName)) {
      return delegate_->legacyMakeTypeface(alias, style);
    }
    return nullptr;
  }

 private:
  sk_sp<SkFontMgr> delegate_;
  // Layout runs on the UI thread and the raster thread resolves typefaces of
  // its own, so the cache is shared. Uncontended in practice -- a fallback
  // lookup happens once per codepoint for the life of the process.
  mutable std::mutex mutex_;
  mutable std::unordered_map<SkUnichar, sk_sp<SkTypeface>> cache_;
};

}  // namespace

std::vector<std::string> GetDefaultFontFamilies() {
  // The generic implementation answers "Arial", which exists on no platform
  // this project will ever run on. Roboto is what ships in kFontDirectory, and
  // it is also what the Material defaults ask for, so naming it here means the
  // fallback path and the common path resolve to the same file.
  return {"Roboto"};
}

sk_sp<SkFontMgr> GetDefaultFontManager(uint32_t font_initialization_data) {
  // Built once. The directory manager scans on construction, and on a memory
  // card that is slow enough to be worth not repeating.
  static sk_sp<SkFontMgr> mgr = sk_make_sp<VitaFallbackFontMgr>(
      SkFontMgr_New_Custom_Directory(kFontDirectory));
  return mgr;
}

}  // namespace txt
