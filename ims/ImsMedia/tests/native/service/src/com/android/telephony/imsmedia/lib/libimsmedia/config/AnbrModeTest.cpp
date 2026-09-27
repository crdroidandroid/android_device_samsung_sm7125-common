/**
 * Copyright (C) 2024 The Android Open Source Project
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

#include <AnbrMode.h>
#include <gtest/gtest.h>

using namespace android::telephony::imsmedia;
using namespace android;

const int32_t kAnbrUplinkMode = 1;
const int32_t kAnbrDownlinkMode = 2;

TEST(AnbrModeTest, TestConstructor)
{
    AnbrMode mode;
    EXPECT_EQ(mode.getAnbrUplinkCodecMode(), 0);
    EXPECT_EQ(mode.getAnbrDownlinkCodecMode(), 0);

    AnbrMode* mode2 = new AnbrMode(mode);
    EXPECT_EQ(mode, *mode2);
    delete mode2;
}

TEST(AnbrModeTest, TestDefault)
{
    AnbrMode mode;
    mode.setDefaultAnbrMode();
    EXPECT_EQ(mode.getAnbrUplinkCodecMode(), 0);
    EXPECT_EQ(mode.getAnbrDownlinkCodecMode(), 0);
}

TEST(AnbrModeTest, TestGetterSetter)
{
    AnbrMode mode;
    mode.setAnbrUplinkCodecMode(kAnbrUplinkMode);
    mode.setAnbrDownlinkCodecMode(kAnbrDownlinkMode);
    EXPECT_EQ(mode.getAnbrUplinkCodecMode(), kAnbrUplinkMode);
    EXPECT_EQ(mode.getAnbrDownlinkCodecMode(), kAnbrDownlinkMode);
}

TEST(AnbrModeTest, TestParcel)
{
    AnbrMode mode1;
    mode1.setAnbrUplinkCodecMode(kAnbrUplinkMode);
    mode1.setAnbrDownlinkCodecMode(kAnbrDownlinkMode);

    android::Parcel parcel;
    EXPECT_EQ(mode1.writeToParcel(nullptr), BAD_VALUE);
    EXPECT_EQ(mode1.writeToParcel(&parcel), NO_ERROR);
    parcel.setDataPosition(0);

    AnbrMode mode2;
    EXPECT_EQ(mode2.readFromParcel(nullptr), BAD_VALUE);
    EXPECT_EQ(mode2.readFromParcel(&parcel), NO_ERROR);
    EXPECT_EQ(mode1, mode2);
}

TEST(AnbrModeTest, TestAssign)
{
    AnbrMode mode1;
    mode1.setAnbrUplinkCodecMode(kAnbrUplinkMode);
    mode1.setAnbrDownlinkCodecMode(kAnbrDownlinkMode);

    AnbrMode mode2;
    mode2 = mode1;
    EXPECT_EQ(mode1, mode2);
}

TEST(AnbrModeTest, TestEqual)
{
    AnbrMode mode1;
    mode1.setAnbrUplinkCodecMode(kAnbrUplinkMode);
    mode1.setAnbrDownlinkCodecMode(kAnbrDownlinkMode);

    AnbrMode mode2;
    mode2.setAnbrUplinkCodecMode(kAnbrUplinkMode);
    mode2.setAnbrDownlinkCodecMode(kAnbrDownlinkMode);
    EXPECT_EQ(mode1, mode2);
}

TEST(AnbrModeTest, TestNotEqual)
{
    AnbrMode mode1;
    mode1.setAnbrUplinkCodecMode(kAnbrUplinkMode);
    mode1.setAnbrDownlinkCodecMode(kAnbrDownlinkMode);

    AnbrMode mode2;
    mode2.setAnbrUplinkCodecMode(3);
    mode2.setAnbrDownlinkCodecMode(kAnbrDownlinkMode);

    AnbrMode mode3;
    mode3.setAnbrUplinkCodecMode(kAnbrUplinkMode);
    mode3.setAnbrDownlinkCodecMode(4);

    EXPECT_NE(mode1, mode2);
    EXPECT_NE(mode1, mode3);
}
