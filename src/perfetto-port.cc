// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "perfetto-port.h"

namespace {
    
using perfetto::protos::gen::TrackEventConfig;

enum class MatchType { kExact, kPattern };

static bool NameMatchesPattern(const std::string& pattern,
                               const std::string& name,
                               MatchType match_type) {
  // To avoid pulling in all of std::regex, for now we only support a single "*"
  // wildcard at the end of the pattern.
  size_t i = pattern.find('*');
  if (i != std::string::npos) {
    PERFETTO_DCHECK(i == pattern.size() - 1);
    if (match_type != MatchType::kPattern)
      return false;
    return name.substr(0, i) == pattern.substr(0, i);
  }
  return name == pattern;
}

static bool NameMatchesPatternList(const std::vector<std::string>& patterns,
                                   const std::string& name,
                                   MatchType match_type) {
  for (const auto& pattern : patterns) {
    if (NameMatchesPattern(pattern, name, match_type))
      return true;
  }
  return false;
}

}  // anonymous namespace

bool IsCategoryEnabled(const percetto_category& category,
                       const TrackEventConfig& config) {
  // Ported from TrackEventInternal::IsCategoryEnabled.
  auto has_matching_tag = [&](std::function<bool(const char*)> matcher) {
    if (category.flags & PERCETTO_CATEGORY_FLAG_SLOW) {
      if (matcher("slow"))
        return true;
    }
    if (category.flags & PERCETTO_CATEGORY_FLAG_DEBUG) {
      if (matcher("debug"))
        return true;
    }
    return false;
  };

  // First try exact matches, then pattern matches.
  const std::array<MatchType, 2> match_types = {
      {MatchType::kExact, MatchType::kPattern}};
  for (auto match_type : match_types) {
    // 1. Enabled categories.
    if (NameMatchesPatternList(config.enabled_categories(), category.name,
                               match_type)) {
      return true;
    }

    // 2. Enabled tags.
    if (has_matching_tag([&](const char* tag) {
          return NameMatchesPatternList(config.enabled_tags(), tag, match_type);
        })) {
      return true;
    }

    // 3. Disabled categories.
    if (NameMatchesPatternList(config.disabled_categories(), category.name,
                               match_type)) {
      return false;
    }

    // 4. Disabled tags.
    if (has_matching_tag([&](const char* tag) {
          if (config.disabled_tags_size()) {
            return NameMatchesPatternList(config.disabled_tags(), tag,
                                          match_type);
          } else {
            // The "slow" and "debug" tags are disabled by default.
            return NameMatchesPattern("slow", tag, match_type) ||
                   NameMatchesPattern("debug", tag, match_type);
          }
        })) {
      return false;
    }
  }

  // If nothing matched, enable the category by default.
  return true;
}
