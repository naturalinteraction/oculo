/************************************************************************************

Filename    :   MainActivity.java
Content     :   
Created     :   
Authors     :   

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/
package com.oculus.vrcontroller;

import android.os.Bundle;
import android.util.Log;
import android.content.Intent;
import com.oculus.vrappframework.VrActivity;

public class MainActivity extends VrActivity {
	public static final String TAG = "VrController";

	/** Load jni .so on initialization */
	static {
		Log.d(TAG, "LoadLibrary");
		System.loadLibrary("vrcontroller");
	}

    public static native long nativeSetAppInterface( VrActivity act, String fromPackageNameString, String commandString, String uriString );

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

		Intent intent = getIntent();
		String commandString = VrActivity.getCommandStringFromIntent( intent );
		String fromPackageNameString = VrActivity.getPackageStringFromIntent( intent );
		String uriString = VrActivity.getUriStringFromIntent( intent );

		setAppPtr( nativeSetAppInterface( this, fromPackageNameString, commandString, uriString ) );
    }   
}
