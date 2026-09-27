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
import static org.junit.Assert.fail;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.verify;

import android.os.IBinder;
import android.os.RemoteException;
import android.telephony.ims.RtpHeaderExtension;
import android.view.Surface;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.List;

public class ImsVideoSessionTest {
    private static final int SESSION_ID = 1;
    @Mock
    private IImsVideoSession mMockIImsVideoSession;
    @Mock
    private IBinder mMockBinder;
    @Mock
    private Surface mMockPreviewSurface;
    @Mock
    private Surface mMockDisplaySurface;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testConstructorAndApis() {
        try {
            doReturn(mMockBinder).when(mMockIImsVideoSession).asBinder();
            doReturn(SESSION_ID).when(mMockIImsVideoSession).getSessionId();
        } catch (Exception e) {
            fail(e.getMessage());
        }

        ImsVideoSession imsVideoSession = new ImsVideoSession(mMockIImsVideoSession);
        assertEquals(mMockBinder, imsVideoSession.getBinder());
        assertEquals(SESSION_ID, imsVideoSession.getSessionId());

        VideoConfig videoConfig = new VideoConfig.Builder().build();
        imsVideoSession.modifySession(videoConfig);

        imsVideoSession.setPreviewSurface(mMockPreviewSurface);
        imsVideoSession.setDisplaySurface(mMockDisplaySurface);

        imsVideoSession.requestVideoDataUsage();

        MediaQualityThreshold threshold = new MediaQualityThreshold.Builder().build();
        imsVideoSession.setMediaQualityThreshold(threshold);

        List<RtpHeaderExtension> extensions = new ArrayList<RtpHeaderExtension>();
        imsVideoSession.sendHeaderExtension(extensions);
        imsVideoSession.requestRtpReceptionStats(1000);
        imsVideoSession.adjustDelay(100);

        try {
            verify(mMockIImsVideoSession).asBinder();
            verify(mMockIImsVideoSession).getSessionId();
            verify(mMockIImsVideoSession).modifySession(videoConfig);
            verify(mMockIImsVideoSession).setPreviewSurface(mMockPreviewSurface);
            verify(mMockIImsVideoSession).setDisplaySurface(mMockDisplaySurface);
            verify(mMockIImsVideoSession).requestVideoDataUsage();
            verify(mMockIImsVideoSession).setMediaQualityThreshold(threshold);
            verify(mMockIImsVideoSession).sendHeaderExtension(extensions);
            verify(mMockIImsVideoSession).requestRtpReceptionStats(1000);
            verify(mMockIImsVideoSession).adjustDelay(100);
        } catch (Exception e) {
            fail(e.getMessage());
        }
    }

    @Test
    public void testApiExceptions() throws Exception {
        ImsVideoSession imsVideoSession = new ImsVideoSession(mMockIImsVideoSession);
        VideoConfig videoConfig = new VideoConfig.Builder().build();
        MediaQualityThreshold threshold = new MediaQualityThreshold.Builder().build();
        List<RtpHeaderExtension> extensions = new ArrayList<>();

        doThrow(new RemoteException()).when(mMockIImsVideoSession).getSessionId();
        imsVideoSession.getSessionId();

        doThrow(new RemoteException()).when(mMockIImsVideoSession).modifySession(videoConfig);
        imsVideoSession.modifySession(videoConfig);

        doThrow(new RemoteException())
            .when(mMockIImsVideoSession).setPreviewSurface(mMockPreviewSurface);
        imsVideoSession.setPreviewSurface(mMockPreviewSurface);

        doThrow(new RemoteException())
            .when(mMockIImsVideoSession).setDisplaySurface(mMockDisplaySurface);
        imsVideoSession.setDisplaySurface(mMockDisplaySurface);

        doThrow(new RemoteException())
            .when(mMockIImsVideoSession).setMediaQualityThreshold(threshold);
        imsVideoSession.setMediaQualityThreshold(threshold);

        doThrow(new RemoteException()).when(mMockIImsVideoSession).sendHeaderExtension(extensions);
        imsVideoSession.sendHeaderExtension(extensions);

        doThrow(new RemoteException()).when(mMockIImsVideoSession).requestVideoDataUsage();
        imsVideoSession.requestVideoDataUsage();

        doThrow(new RemoteException()).when(mMockIImsVideoSession).requestRtpReceptionStats(1000);
        imsVideoSession.requestRtpReceptionStats(1000);

        doThrow(new RemoteException()).when(mMockIImsVideoSession).adjustDelay(100);
        imsVideoSession.adjustDelay(100);
    }
}
