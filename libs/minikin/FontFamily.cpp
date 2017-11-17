/*
 * Copyright (C) 2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "Minikin"

#include "minikin/FontFamily.h"

#include <cstdint>
#include <vector>

#include <log/log.h>
#include <utils/JenkinsHash.h>

#include "minikin/CmapCoverage.h"
#include "minikin/MinikinFont.h"
#include "FontUtils.h"
#include "Locale.h"
#include "LocaleListCache.h"
#include "MinikinInternal.h"

namespace minikin {

Font::Font(const std::shared_ptr<MinikinFont>& typeface, FontStyle style)
    : typeface(typeface), style(style) {
}

Font::Font(std::shared_ptr<MinikinFont>&& typeface, FontStyle style)
    : typeface(typeface), style(style) {
}

std::unordered_set<AxisTag> Font::getSupportedAxesLocked() const {
    const uint32_t fvarTag = MinikinFont::MakeTag('f', 'v', 'a', 'r');
    HbBlob fvarTable(getFontTable(typeface.get(), fvarTag));
    if (fvarTable.size() == 0) {
        return std::unordered_set<AxisTag>();
    }

    std::unordered_set<AxisTag> supportedAxes;
    analyzeAxes(fvarTable.get(), fvarTable.size(), &supportedAxes);
    return supportedAxes;
}

Font::Font(Font&& o) {
    typeface = std::move(o.typeface);
    style = o.style;
    o.typeface = nullptr;
}

Font::Font(const Font& o) {
    typeface = o.typeface;
    style = o.style;
}

// static
FontFamily::FontFamily(std::vector<Font>&& fonts)
      : FontFamily(Variant::DEFAULT, std::move(fonts)) {
}

FontFamily::FontFamily(Variant variant, std::vector<Font>&& fonts)
    : FontFamily(LocaleListCache::kEmptyListId, variant, std::move(fonts)) {
}

FontFamily::FontFamily(uint32_t localeListId, Variant variant, std::vector<Font>&& fonts)
    : mLocaleListId(localeListId), mVariant(variant), mFonts(std::move(fonts)) {
    computeCoverage();
}

bool FontFamily::analyzeStyle(const std::shared_ptr<MinikinFont>& typeface, int* weight,
        bool* italic) {
    android::AutoMutex _l(gMinikinLock);
    const uint32_t os2Tag = MinikinFont::MakeTag('O', 'S', '/', '2');
    HbBlob os2Table(getFontTable(typeface.get(), os2Tag));
    if (os2Table.get() == nullptr) return false;
    return ::minikin::analyzeStyle(os2Table.get(), os2Table.size(), weight, italic);
}

// Compute a matching metric between two styles - 0 is an exact match
static int computeMatch(FontStyle style1, FontStyle style2) {
    if (style1 == style2) return 0;
    int score = abs(style1.weight() / 100 - style2.weight() / 100);
    if (style1.slant() != style2.slant()) {
        score += 2;
    }
    return score;
}

static FontFakery computeFakery(FontStyle wanted, FontStyle actual) {
    // If desired weight is semibold or darker, and 2 or more grades
    // higher than actual (for example, medium 500 -> bold 700), then
    // select fake bold.
    bool isFakeBold = wanted.weight() >= 600 && (wanted.weight() - actual.weight()) >= 200;
    bool isFakeItalic = wanted.slant() == FontStyle::Slant::ITALIC &&
            actual.slant() == FontStyle::Slant::UPRIGHT;
    return FontFakery(isFakeBold, isFakeItalic);
}

FakedFont FontFamily::getClosestMatch(FontStyle style) const {
    const Font* bestFont = nullptr;
    int bestMatch = 0;
    for (size_t i = 0; i < mFonts.size(); i++) {
        const Font& font = mFonts[i];
        int match = computeMatch(font.style, style);
        if (i == 0 || match < bestMatch) {
            bestFont = &font;
            bestMatch = match;
        }
    }
    if (bestFont != nullptr) {
        return FakedFont{ bestFont->typeface.get(), computeFakery(style, bestFont->style) };
    }
    return FakedFont{ nullptr, FontFakery() };
}

bool FontFamily::isColorEmojiFamily() const {
    const LocaleList& localeList = LocaleListCache::getById(mLocaleListId);
    for (size_t i = 0; i < localeList.size(); ++i) {
        if (localeList[i].getEmojiStyle() == Locale::EMSTYLE_EMOJI) {
            return true;
        }
    }
    return false;
}

void FontFamily::computeCoverage() {
    android::AutoMutex _l(gMinikinLock);
    const FontStyle defaultStyle;
    const MinikinFont* typeface = getClosestMatch(defaultStyle).font;
    const uint32_t cmapTag = MinikinFont::MakeTag('c', 'm', 'a', 'p');
    HbBlob cmapTable(getFontTable(typeface, cmapTag));
    if (cmapTable.get() == nullptr) {
        ALOGE("Could not get cmap table size!\n");
        return;
    }
    mCoverage = CmapCoverage::getCoverage(cmapTable.get(), cmapTable.size(), &mCmapFmt14Coverage);

    for (size_t i = 0; i < mFonts.size(); ++i) {
        std::unordered_set<AxisTag> supportedAxes = mFonts[i].getSupportedAxesLocked();
        mSupportedAxes.insert(supportedAxes.begin(), supportedAxes.end());
    }
}

bool FontFamily::hasGlyph(uint32_t codepoint, uint32_t variationSelector) const {
    if (variationSelector == 0) {
        return mCoverage.get(codepoint);
    }

    if (mCmapFmt14Coverage.empty()) {
        return false;
    }

    const uint16_t vsIndex = getVsIndex(variationSelector);

    if (vsIndex >= mCmapFmt14Coverage.size()) {
        // Even if vsIndex is INVALID_VS_INDEX, we reach here since INVALID_VS_INDEX is defined to
        // be at the maximum end of the range.
        return false;
    }

    const std::unique_ptr<SparseBitSet>& bitset = mCmapFmt14Coverage[vsIndex];
    if (bitset.get() == nullptr) {
        return false;
    }

    return bitset->get(codepoint);
}

std::shared_ptr<FontFamily> FontFamily::createFamilyWithVariation(
        const std::vector<FontVariation>& variations) const {
    if (variations.empty() || mSupportedAxes.empty()) {
        return nullptr;
    }

    bool hasSupportedAxis = false;
    for (const FontVariation& variation : variations) {
        if (mSupportedAxes.find(variation.axisTag) != mSupportedAxes.end()) {
            hasSupportedAxis = true;
            break;
        }
    }
    if (!hasSupportedAxis) {
        // None of variation axes are suppored by this family.
        return nullptr;
    }

    std::vector<Font> fonts;
    for (const Font& font : mFonts) {
        bool supportedVariations = false;
        android::AutoMutex _l(gMinikinLock);
        std::unordered_set<AxisTag> supportedAxes = font.getSupportedAxesLocked();
        if (!supportedAxes.empty()) {
            for (const FontVariation& variation : variations) {
                if (supportedAxes.find(variation.axisTag) != supportedAxes.end()) {
                    supportedVariations = true;
                    break;
                }
            }
        }
        std::shared_ptr<MinikinFont> minikinFont;
        if (supportedVariations) {
            minikinFont = font.typeface->createFontWithVariation(variations);
        }
        if (minikinFont == nullptr) {
            minikinFont = font.typeface;
        }
        fonts.push_back(Font(std::move(minikinFont), font.style));
    }

    return std::shared_ptr<FontFamily>(new FontFamily(mLocaleListId, mVariant, std::move(fonts)));
}

}  // namespace minikin
