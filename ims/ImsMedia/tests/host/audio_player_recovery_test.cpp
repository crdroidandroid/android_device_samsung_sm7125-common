// SPDX-License-Identifier: Apache-2.0
#include <gtest/gtest.h>
#include <ImsMediaDefine.h>
#include <ImsMediaAudioPlayer.h>
#include "fake_audio_platform.h"

class AudioPlayerRecoveryTest : public ::testing::Test {
protected:
    void SetUp() override { FakeAudio::reset(); }
    static bool play(ImsMediaAudioPlayer& player) {
        uint8_t frame[] = {0, 1};
        return player.onDataFrame(frame, sizeof(frame), SPEECH, false, 0);
    }
    static void start(ImsMediaAudioPlayer& player) {
        player.SetCodec(kAudioCodecL16);
        ASSERT_TRUE(player.Start());
        ASSERT_TRUE(play(player));
    }
};

TEST_F(AudioPlayerRecoveryTest, FailedReopenRetriesOnLaterFrames) {
    ImsMediaAudioPlayer player; start(player);
    FakeAudio::disconnect();
    FakeAudio::failOpen = 2; // exclusive and shared fail during one reopen attempt
    EXPECT_FALSE(play(player));
    int attempts = FakeAudio::openAttempts;
    EXPECT_FALSE(play(player));
    EXPECT_EQ(FakeAudio::openAttempts, attempts); // no busy retry on every packet
    FakeAudio::nowUs += 100000;
    EXPECT_TRUE(play(player));
    EXPECT_GT(FakeAudio::openAttempts, attempts);
}

TEST_F(AudioPlayerRecoveryTest, FailedRestartAndStartWaitRemainRecoverable) {
    ImsMediaAudioPlayer player; start(player);
    FakeAudio::disconnect(); FakeAudio::failStart = 1;
    EXPECT_FALSE(play(player));
    FakeAudio::nowUs += 100000; FakeAudio::failWait = 1;
    EXPECT_FALSE(play(player));
    FakeAudio::nowUs += 100000;
    EXPECT_TRUE(play(player));
}

TEST_F(AudioPlayerRecoveryTest, StopCancelsPendingRecovery) {
    ImsMediaAudioPlayer player; start(player);
    FakeAudio::disconnect(); FakeAudio::failOpen = 2;
    EXPECT_FALSE(play(player)); player.Stop();
    int attempts = FakeAudio::openAttempts;
    FakeAudio::nowUs += 1000000;
    EXPECT_FALSE(play(player));
    EXPECT_EQ(FakeAudio::openAttempts, attempts);
}

TEST_F(AudioPlayerRecoveryTest, StaleDisconnectDoesNotReplaceTheCurrentStream) {
    ImsMediaAudioPlayer player; start(player);
    auto* old = FakeAudio::current(); FakeAudio::disconnect();
    EXPECT_TRUE(play(player));
    auto* current = FakeAudio::current(); ASSERT_NE(current, old);
    old->error(old, old->user, AAUDIO_ERROR_DISCONNECTED);
    EXPECT_TRUE(play(player));
    EXPECT_EQ(FakeAudio::current(), current);
    EXPECT_FALSE(current->closed);
}

TEST_F(AudioPlayerRecoveryTest, FailedInitialStartDoesNotStartFromFrames) {
    ImsMediaAudioPlayer player;
    player.SetCodec(kAudioCodecL16);
    FakeAudio::failOpen = 2;
    ASSERT_FALSE(player.Start());
    int attempts = FakeAudio::openAttempts;
    FakeAudio::nowUs += 1000000;
    EXPECT_FALSE(play(player));
    EXPECT_EQ(FakeAudio::openAttempts, attempts);
    EXPECT_TRUE(player.Start());
    EXPECT_TRUE(play(player));
}

TEST_F(AudioPlayerRecoveryTest, StartCannotBypassPendingRecovery) {
    ImsMediaAudioPlayer player; start(player);
    FakeAudio::disconnect(); FakeAudio::failOpen = 2;
    EXPECT_FALSE(play(player));
    int attempts = FakeAudio::openAttempts;
    EXPECT_FALSE(player.Start());
    EXPECT_EQ(FakeAudio::openAttempts, attempts);
    FakeAudio::nowUs += 100000;
    EXPECT_TRUE(play(player));
}
