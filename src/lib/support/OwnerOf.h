/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace chip {

template<class ClassType, class MemeberType>
static inline constexpr std::ptrdiff_t offset_of(const MemeberType ClassType::*member) {
    return reinterpret_cast<std::ptrdiff_t>(&(reinterpret_cast<ClassType*>(0)->*member));
}

template<class ClassType, class MemeberType>
static inline constexpr ClassType* owner_of(MemeberType *ptr, const MemeberType ClassType::*member) {
    return reinterpret_cast<ClassType*>(reinterpret_cast<intptr_t>(ptr) - offset_of(member));
}

}
