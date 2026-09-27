/**
 * Copyright (C) 2022 The Android Open Source Project
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
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.IBinder;
import android.os.RemoteException;
import android.telephony.ims.RtpHeaderExtension;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link ImsAudioSession}.
 */
@RunWith(JUnit4.class)
public class ImsAudioSessionTest {

    private ImsAudioSession mImsAudioSession;

    @Mock
    private IImsAudioSession mMockAudioSession;
    @Mock
    private IBinder mMockBinder;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockAudioSession.asBinder()).thenReturn(mMockBinder);
        mImsAudioSession = new ImsAudioSession(mMockAudioSession);
    }

    @Test
    public void testGetBinder() {
        assertEquals(mMockBinder, mImsAudioSession.getBinder());
    }

    @Test
    public void testGetSessionId() throws RemoteException {
        when(mMockAudioSession.getSessionId()).thenReturn(1);
        assertEquals(1, mImsAudioSession.getSessionId());
        verify(mMockAudioSession).getSessionId();
    }

    @Test
    public void testGetSessionIdWithException() throws RemoteException {
        doThrow(new RemoteException()).when(mMockAudioSession).getSessionId();
        assertEquals(-1, mImsAudioSession.getSessionId());
    }

    @Test
    public void testModifySession() throws RemoteException {
        AudioConfig config = new AudioConfig.Builder().build();
        mImsAudioSession.modifySession(config);
        verify(mMockAudioSession).modifySession(config);
    }

    @Test
    public void testModifySessionWithException() throws RemoteException {
        AudioConfig config = new AudioConfig.Builder().build();
        doThrow(new RemoteException())
                .when(mMockAudioSession).modifySession(any(AudioConfig.class));
        mImsAudioSession.modifySession(config);
        // Verify method call and no exception is thrown
        verify(mMockAudioSession).modifySession(config);
    }

    @Test
    public void testSetMediaQualityThreshold() throws RemoteException {
        MediaQualityThreshold threshold = new MediaQualityThreshold.Builder().build();
        mImsAudioSession.setMediaQualityThreshold(threshold);
        verify(mMockAudioSession).setMediaQualityThreshold(threshold);
    }

    @Test
    public void testSetMediaQualityThresholdWithException() throws RemoteException {
        MediaQualityThreshold threshold = new MediaQualityThreshold.Builder().build();
        doThrow(new RemoteException()).when(mMockAudioSession).setMediaQualityThreshold(
                any(MediaQualityThreshold.class));
        mImsAudioSession.setMediaQualityThreshold(threshold);
        verify(mMockAudioSession).setMediaQualityThreshold(threshold);
    }

    @Test
    public void testAddConfig() throws RemoteException {
        AudioConfig config = new AudioConfig.Builder().build();
        mImsAudioSession.addConfig(config);
        verify(mMockAudioSession).addConfig(config);
    }

    @Test
    public void testAddConfigWithException() throws RemoteException {
        AudioConfig config = new AudioConfig.Builder().build();
        doThrow(new RemoteException()).when(mMockAudioSession).addConfig(any(AudioConfig.class));
        mImsAudioSession.addConfig(config);
        verify(mMockAudioSession).addConfig(config);
    }

    @Test
    public void testDeleteConfig() throws RemoteException {
        AudioConfig config = new AudioConfig.Builder().build();
        mImsAudioSession.deleteConfig(config);
        verify(mMockAudioSession).deleteConfig(config);
    }

    @Test
    public void testDeleteConfigWithException() throws RemoteException {
        AudioConfig config = new AudioConfig.Builder().build();
        doThrow(new RemoteException())
                .when(mMockAudioSession).deleteConfig(any(AudioConfig.class));
        mImsAudioSession.deleteConfig(config);
        verify(mMockAudioSession).deleteConfig(config);
    }

    @Test
    public void testConfirmConfig() throws RemoteException {
        AudioConfig config = new AudioConfig.Builder().build();
        mImsAudioSession.confirmConfig(config);
        verify(mMockAudioSession).confirmConfig(config);
    }

    @Test
    public void testConfirmConfigWithException() throws RemoteException {
        AudioConfig config = new AudioConfig.Builder().build();
        doThrow(new RemoteException())
                .when(mMockAudioSession).confirmConfig(any(AudioConfig.class));
        mImsAudioSession.confirmConfig(config);
        verify(mMockAudioSession).confirmConfig(config);
    }

    @Test
    public void testSendDtmf() throws RemoteException {
        mImsAudioSession.sendDtmf('1', 100);
        verify(mMockAudioSession).sendDtmf('1', 100);
    }

    @Test
    public void testSendDtmfWithException() throws RemoteException {
        doThrow(new RemoteException()).when(mMockAudioSession).sendDtmf('1', 100);
        mImsAudioSession.sendDtmf('1', 100);
        verify(mMockAudioSession).sendDtmf('1', 100);
    }

    @Test
    public void testStartDtmf() throws RemoteException {
        mImsAudioSession.startDtmf('1');
        verify(mMockAudioSession).startDtmf('1');
    }

    @Test
    public void testStartDtmfWithException() throws RemoteException {
        doThrow(new RemoteException()).when(mMockAudioSession).startDtmf('1');
        mImsAudioSession.startDtmf('1');
        verify(mMockAudioSession).startDtmf('1');
    }

    @Test
    public void testStopDtmf() throws RemoteException {
        mImsAudioSession.stopDtmf();
        verify(mMockAudioSession).stopDtmf();
    }

    @Test
    public void testStopDtmfWithException() throws RemoteException {
        doThrow(new RemoteException()).when(mMockAudioSession).stopDtmf();
        mImsAudioSession.stopDtmf();
        verify(mMockAudioSession).stopDtmf();
    }

    @Test
    public void testSendHeaderExtension() throws RemoteException {
        List<RtpHeaderExtension> extensions = new ArrayList<>();
        mImsAudioSession.sendHeaderExtension(extensions);
        verify(mMockAudioSession).sendHeaderExtension(extensions);
    }

    @Test
    public void testSendHeaderExtensionWithException() throws RemoteException {
        List<RtpHeaderExtension> extensions = new ArrayList<>();
        doThrow(new RemoteException()).when(mMockAudioSession).sendHeaderExtension(any());
        mImsAudioSession.sendHeaderExtension(extensions);
        verify(mMockAudioSession).sendHeaderExtension(extensions);
    }

    @Test
    public void testRequestRtpReceptionStats() throws RemoteException {
        mImsAudioSession.requestRtpReceptionStats(1000);
        verify(mMockAudioSession).requestRtpReceptionStats(1000);
    }

    @Test
    public void testRequestRtpReceptionStatsWithException() throws RemoteException {
        doThrow(new RemoteException()).when(mMockAudioSession).requestRtpReceptionStats(1000);
        mImsAudioSession.requestRtpReceptionStats(1000);
        verify(mMockAudioSession).requestRtpReceptionStats(1000);
    }

    @Test
    public void testAdjustDelay() throws RemoteException {
        mImsAudioSession.adjustDelay(100);
        verify(mMockAudioSession).adjustDelay(100);
    }

    @Test
    public void testAdjustDelayWithException() throws RemoteException {
        doThrow(new RemoteException()).when(mMockAudioSession).adjustDelay(100);
        mImsAudioSession.adjustDelay(100);
        verify(mMockAudioSession).adjustDelay(100);
    }
}
