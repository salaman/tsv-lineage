/*
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.tsvsettings;

import android.os.Bundle;
import android.os.SystemProperties;

import androidx.preference.ListPreference;
import androidx.preference.PreferenceFragmentCompat;

import com.android.settingslib.collapsingtoolbar.CollapsingToolbarBaseActivity;

/** Lets the user choose what the mic-mute button and privacy slider do (read by vendor tsv-buttons). */
public class ButtonsActivity extends CollapsingToolbarBaseActivity {

    static final String PROP_MUTE = "persist.sys.tsv.mute_action";
    static final String PROP_SLIDER = "persist.sys.tsv.slider_action";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // SettingsLib base activity: Settings look, collapsing toolbar, edge-to-edge insets handled.
        if (savedInstanceState == null) {
            getSupportFragmentManager().beginTransaction()
                    .replace(com.android.settingslib.collapsingtoolbar.R.id.content_frame,
                            new ButtonsFragment())
                    .commit();
        }
    }

    public static class ButtonsFragment extends PreferenceFragmentCompat {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            setPreferencesFromResource(R.xml.buttons, rootKey);
            bind("mute_action", PROP_MUTE);
            bind("slider_action", PROP_SLIDER);
        }

        private void bind(String key, String prop) {
            ListPreference pref = findPreference(key);
            if (pref == null) return;
            pref.setValue(SystemProperties.get(prop, "power"));
            pref.setSummaryProvider(ListPreference.SimpleSummaryProvider.getInstance());
            pref.setOnPreferenceChangeListener((p, value) -> {
                SystemProperties.set(prop, (String) value);
                return true;
            });
        }
    }
}
