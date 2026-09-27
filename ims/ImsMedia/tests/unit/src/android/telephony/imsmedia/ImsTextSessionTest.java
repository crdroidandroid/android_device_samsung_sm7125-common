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
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.IBinder;
import android.os.RemoteException;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

/**
 * Unit tests for {@link ImsTextSession}.
 */
@RunWith(JUnit4.class)
public class ImsTextSessionTest {

    private static final int SESSION_ID = 1;
    @Mock private IImsTextSession mMockTextSession;
    @Mock private IBinder mMockBinder;
    private ImsTextSession mImsTextSession;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockTextSession.asBinder()).thenReturn(mMockBinder);
        mImsTextSession = new ImsTextSession(mMockTextSession);
    }

    @Test
    public void testGetBinder() {
        assertEquals(mMockBinder, mImsTextSession.getBinder());
    }

    @Test
    public void testGetSessionId() throws RemoteException {
        when(mMockTextSession.getSessionId()).thenReturn(SESSION_ID);
        assertEquals(SESSION_ID, mImsTextSession.getSessionId());
        verify(mMockTextSession).getSessionId();
    }

    @Test
    public void testGetSessionIdRemoteException() throws RemoteException {
        doThrow(new RemoteException()).when(mMockTextSession).getSessionId();
        assertEquals(-1, mImsTextSession.getSessionId());
        verify(mMockTextSession).getSessionId();
    }

    @Test
    public void testModifySession() throws RemoteException {
        TextConfig config = new TextConfig.Builder().build();
        mImsTextSession.modifySession(config);
        verify(mMockTextSession).modifySession(config);
    }

    @Test
    public void testModifySessionRemoteException() throws RemoteException {
        TextConfig config = new TextConfig.Builder().build();
        doThrow(new RemoteException()).when(mMockTextSession).modifySession(config);
        mImsTextSession.modifySession(config);
        verify(mMockTextSession).modifySession(config);
    }

    @Test
    public void testSetMediaQualityThreshold() throws RemoteException {
        MediaQualityThreshold threshold = new MediaQualityThreshold.Builder().build();
        mImsTextSession.setMediaQualityThreshold(threshold);
        verify(mMockTextSession).setMediaQualityThreshold(threshold);
    }

    @Test
    public void testSetMediaQualityThresholdRemoteException() throws RemoteException {
        MediaQualityThreshold threshold = new MediaQualityThreshold.Builder().build();
        doThrow(new RemoteException()).when(mMockTextSession).setMediaQualityThreshold(threshold);
        mImsTextSession.setMediaQualityThreshold(threshold);
        verify(mMockTextSession).setMediaQualityThreshold(threshold);
    }

    @Test
    public void testSendRtt() throws RemoteException {
        String text = "test";
        mImsTextSession.sendRtt(text);
        verify(mMockTextSession).sendRtt(text);
    }

    @Test
    public void testSendRttRemoteException() throws RemoteException {
        String text = "test";
        doThrow(new RemoteException()).when(mMockTextSession).sendRtt(text);
        mImsTextSession.sendRtt(text);
        verify(mMockTextSession).sendRtt(text);
    }
}
