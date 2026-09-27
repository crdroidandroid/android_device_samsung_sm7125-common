/*
 * Copyright (C) 2023 The Android Open Source Project
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

package com.android.telephony.imsmedia;

import static org.junit.Assert.fail;

import android.testing.TestableLooper;

import androidx.test.platform.app.InstrumentationRegistry;

import java.util.ArrayList;
import java.util.List;

public abstract class ImsMediaTest {
    protected List<TestableLooper> mTestableLoopers = new ArrayList<>();
    protected TestableLooper mTestableLooper;
    protected Object mTestClass;

    protected void setUp() {
        ImsMediaApplication.setAppContext(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        mTestableLooper = TestableLooper.get(mTestClass);
        if (mTestableLooper != null) {
            monitorTestableLooper(mTestableLooper);
        }
    }

    protected void tearDown() throws Exception {
        ImsMediaApplication.setAppContext(null);
        WakeLockManager.setInstance(null);
        // Unmonitor the main TestableLooper so it is not quit or destroyed, as it is
        // managed by the TestableLooper runner.
        if (mTestableLooper != null) {
            unmonitorTestableLooper(mTestableLooper);
        }

        // Quit and destroy any other TestableLoopers that were created during the test.
        if (!mTestableLoopers.isEmpty()) {
            for (TestableLooper looper : mTestableLoopers) {
                looper.getLooper().quit();
                looper.destroy();
            }
            mTestableLoopers.clear();
        }
        TestableLooper.remove(mTestClass);
    }

    private void monitorTestableLooper(TestableLooper looper) {
        if (!mTestableLoopers.contains(looper)) {
            mTestableLoopers.add(looper);
        }
    }

    private void unmonitorTestableLooper(TestableLooper looper) {
        if (mTestableLoopers.contains(looper)) {
            mTestableLoopers.remove(looper);
        }
    }

    private boolean areAllTestableLoopersIdle() {
        for (TestableLooper looper : mTestableLoopers) {
            if (!looper.getLooper().getQueue().isIdle()) return false;
        }
        return true;
    }

    public void processAllMessages() {
        if (mTestableLoopers.isEmpty()) {
            fail("mTestableLoopers is empty. Please make sure to add @RunWithLooper annotation");
        }
        while (!areAllTestableLoopersIdle()) {
            for (TestableLooper looper : mTestableLoopers) looper.processAllMessages();
        }
    }
}
