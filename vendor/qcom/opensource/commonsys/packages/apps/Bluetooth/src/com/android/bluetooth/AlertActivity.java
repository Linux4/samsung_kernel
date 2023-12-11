/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.bluetooth;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.view.ViewGroup;
import android.view.Window;
import android.view.accessibility.AccessibilityEvent;

/**
 * An activity that follows the visual style of an AlertDialog.
 *
 * @see #mAlert
 * @see #setupAlert()
 */
@SuppressLint("AndroidFrameworkBluetoothPermission")
public abstract class AlertActivity extends Activity implements DialogInterface.OnDismissListener,
        DialogInterface.OnCancelListener {

    /**
     * The model for the alert.
     *
     */
    protected AlertDialog.Builder mAlertBuilder;
    private AlertDialog mAlert;

    public AlertActivity() {}

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        mAlertBuilder = new AlertDialog.Builder(this);
        mAlertBuilder.setOnDismissListener(this);
        mAlertBuilder.setOnCancelListener(this);
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        if (!isFinishing()) {
            finish();
        }
    }

    @Override
    public void onCancel(DialogInterface dialog) {
        if (!isFinishing()) {
            finish();
        }
    }

    @Override
    public boolean dispatchPopulateAccessibilityEvent(AccessibilityEvent event) {
        return dispatchPopulateAccessibilityEvent(this, event);
    }

    private static boolean dispatchPopulateAccessibilityEvent(Activity act,
            AccessibilityEvent event) {
        event.setClassName(Dialog.class.getName());
        event.setPackageName(act.getPackageName());

        ViewGroup.LayoutParams params = act.getWindow().getAttributes();
        boolean isFullScreen = (params.width == ViewGroup.LayoutParams.MATCH_PARENT)
                && (params.height == ViewGroup.LayoutParams.MATCH_PARENT);
        event.setFullScreen(isFullScreen);

        return false;
    }

    protected void setupAlert() {
        mAlert = mAlertBuilder.create();
        mAlert.show();
    }

    protected void changeIconAttribute(int attrId) {
        if (mAlert == null) return;
        mAlert.setIconAttribute(attrId);
    }
    protected void changeTitle(CharSequence title) {
        if (mAlert == null) return;
        mAlert.setTitle(title);
    }

    protected void changeButtonVisibility(int identifier, int visibility) {
        if (mAlert == null) return;
        mAlert.getButton(identifier).setVisibility(visibility);
    }

    protected void changeButtonText(int identifier, CharSequence text) {
        if (mAlert == null) return;
        mAlert.getButton(identifier).setText(text);
    }

    protected void changeButtonEnabled(int identifier, boolean enable) {
        if (mAlert == null) return;
        mAlert.getButton(identifier).setEnabled(enable);
    }

    @Override
    protected void onDestroy() {
        if (mAlert != null) {
            mAlert.dismiss();
        }
        super.onDestroy();
    }

}
