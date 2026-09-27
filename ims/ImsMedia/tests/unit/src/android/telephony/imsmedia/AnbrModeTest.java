/*
 * Copyright (C) 2025 The Android Open Source Project
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

package android.telephony.imsmedia;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.os.Parcel;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

@RunWith(JUnit4.class)
public class AnbrModeTest {

    private static final int ANBR_UPLINK_MODE = 1;
    private static final int ANBR_DOWNLINK_MODE = 2;

    @Test
    public void testBuilder() {
        AnbrMode anbrMode = new AnbrMode.Builder()
                .setAnbrUplinkCodecMode(ANBR_UPLINK_MODE)
                .setAnbrDownlinkCodecMode(ANBR_DOWNLINK_MODE)
                .build();

        assertEquals(ANBR_UPLINK_MODE, anbrMode.getAnbrUplinkCodecMode());
        assertEquals(ANBR_DOWNLINK_MODE, anbrMode.getAnbrDownlinkCodecMode());
    }

    @Test
    public void testSettersAndGetters() {
        AnbrMode anbrMode = new AnbrMode.Builder().build();

        anbrMode.setAnbrUplinkCodecMode(ANBR_UPLINK_MODE);
        assertEquals(ANBR_UPLINK_MODE, anbrMode.getAnbrUplinkCodecMode());

        anbrMode.setAnbrDownlinkCodecMode(ANBR_DOWNLINK_MODE);
        assertEquals(ANBR_DOWNLINK_MODE, anbrMode.getAnbrDownlinkCodecMode());
    }

    @Test
    public void testParcelable() {
        AnbrMode anbrMode = new AnbrMode.Builder()
                .setAnbrUplinkCodecMode(ANBR_UPLINK_MODE)
                .setAnbrDownlinkCodecMode(ANBR_DOWNLINK_MODE)
                .build();

        Parcel parcel = Parcel.obtain();
        anbrMode.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);

        AnbrMode fromParcel = AnbrMode.CREATOR.createFromParcel(parcel);

        assertEquals(anbrMode, fromParcel);
        parcel.recycle();
    }

    @Test
    public void testEqualsAndHashCode() {
        AnbrMode anbrMode1 = new AnbrMode.Builder()
                .setAnbrUplinkCodecMode(ANBR_UPLINK_MODE)
                .setAnbrDownlinkCodecMode(ANBR_DOWNLINK_MODE)
                .build();

        AnbrMode anbrMode2 = new AnbrMode.Builder()
                .setAnbrUplinkCodecMode(ANBR_UPLINK_MODE)
                .setAnbrDownlinkCodecMode(ANBR_DOWNLINK_MODE)
                .build();

        AnbrMode anbrMode3 = new AnbrMode.Builder()
                .setAnbrUplinkCodecMode(ANBR_UPLINK_MODE + 1)
                .setAnbrDownlinkCodecMode(ANBR_DOWNLINK_MODE)
                .build();

        AnbrMode anbrMode4 = new AnbrMode.Builder()
                .setAnbrUplinkCodecMode(ANBR_UPLINK_MODE)
                .setAnbrDownlinkCodecMode(ANBR_DOWNLINK_MODE + 1)
                .build();

        assertEquals(anbrMode1, anbrMode2);
        assertEquals(anbrMode1.hashCode(), anbrMode2.hashCode());

        assertNotEquals(anbrMode1, anbrMode3);
        assertNotEquals(anbrMode1.hashCode(), anbrMode3.hashCode());

        assertNotEquals(anbrMode2, anbrMode4);
        assertNotEquals(anbrMode1.hashCode(), anbrMode4.hashCode());

        assertNotEquals(anbrMode3, anbrMode4);
        assertNotEquals(anbrMode3.hashCode(), anbrMode4.hashCode());
    }

    @SuppressWarnings("SelfAssertion")
    @Test
    public void testSpecialEqualsCases() {
        AnbrMode anbrMode = new AnbrMode.Builder()
                .setAnbrUplinkCodecMode(ANBR_UPLINK_MODE)
                .setAnbrDownlinkCodecMode(ANBR_DOWNLINK_MODE)
                .build();
        // Test equals with the same object reference
        assertEquals(anbrMode, anbrMode);
        // Test equals with null
        assertNotEquals(anbrMode, null);
        // Test equals with a different object type
        assertNotEquals(anbrMode, new Object());
    }

    @Test
    public void testToString() {
        AnbrMode anbrMode = new AnbrMode.Builder()
                .setAnbrUplinkCodecMode(ANBR_UPLINK_MODE)
                .setAnbrDownlinkCodecMode(ANBR_DOWNLINK_MODE)
                .build();

        String expected = "AnbrMode: {mAnbrUplinkMode =" + ANBR_UPLINK_MODE
                + ", mAnbrDownlinkMode =" + ANBR_DOWNLINK_MODE + " }";
        assertEquals(expected, anbrMode.toString());
    }

    @Test
    public void testDescribeContents() {
        AnbrMode anbrMode = new AnbrMode.Builder().build();
        assertEquals(0, anbrMode.describeContents());
    }

    @Test
    public void testNewArray() {
        final int size = 5;
        AnbrMode[] anbrModes = AnbrMode.CREATOR.newArray(size);
        assertNotNull(anbrModes);
        assertEquals(size, anbrModes.length);
    }
}
