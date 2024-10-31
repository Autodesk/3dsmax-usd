//
// Copyright 2023 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#pragma once

/**
 * \file Some Tuple related utilities.
 *
 * Include this file to have std::hash support for std::tuple
 */

namespace {

template <std::size_t I = 0, typename... Ts>
inline typename std::enable_if<I == sizeof...(Ts), std::size_t>::type
HashTuple(const std::tuple<Ts...>& t)
{
    return 0;
}

template <std::size_t I = 0, typename... Ts>
    inline typename std::enable_if
    < I<sizeof...(Ts), size_t>::type HashTuple(const std::tuple<Ts...>& t)
{
    return std::hash<std::tuple_element_t<I, std::tuple<Ts...>>> {}(std::get<I>(t))
        + HashTuple<I + 1>(t);
}

} // namespace

namespace std {

template <typename... Ts> struct hash<std::tuple<Ts...>>
{
    size_t operator()(const std::tuple<Ts...>& t) const noexcept { return HashTuple(t); }
};

} // namespace std
