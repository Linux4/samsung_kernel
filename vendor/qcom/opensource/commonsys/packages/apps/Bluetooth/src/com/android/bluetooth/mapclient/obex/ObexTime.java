/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.bluetooth.mapclient;

import java.util.Calendar;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class ObexTime {

    private Date mDate;

    public ObexTime(String time) {
        /*
         * Match OBEX time string: YYYYMMDDTHHMMSS with optional UTF offset +/-hhmm
         *
         * Matched groups are numberes as follows:
         *
         *     YYYY MM DD T HH MM SS + hh mm
         *     ^^^^ ^^ ^^   ^^ ^^ ^^ ^ ^^ ^^
         *     1    2  3    4  5  6  8 9  10
         *                          |---7---|
         *
         * All groups are guaranteed to be numeric so conversion will always succeed (except group 8
         * which is either + or -)
         */
        Pattern p = Pattern.compile(
                "(\\d{4})(\\d{2})(\\d{2})T(\\d{2})(\\d{2})(\\d{2})(([+-])(\\d{2})(\\d{2})" + ")?");
        Matcher m = p.matcher(time);

        if (m.matches()) {

            /*
             * MAP spec says to default to "Local Time basis" for a message listing timestamp. We'll
             * use the system default timezone and assume it knows best what our local timezone is.
             * The builder defaults to the default locale and timezone if none is provided.
             */
            Calendar.Builder builder = new Calendar.Builder();

            /* Note that Calendar months are zero-based */
            builder.setDate(Integer.parseInt(m.group(1)), /* year */
                    Integer.parseInt(m.group(2)) - 1,     /* month */
                    Integer.parseInt(m.group(3)));        /* day of month */

            /* Note the MAP timestamp doesn't have milliseconds and we're explicitly setting to 0 */
            builder.setTimeOfDay(Integer.parseInt(m.group(4)), /* hours */
                    Integer.parseInt(m.group(5)),              /* minutes */
                    Integer.parseInt(m.group(6)),              /* seconds */
                    0);                                        /* milliseconds */

            /*
             * If 7th group is matched then we're no longer using "Local Time basis" and instead
             * have a UTC based timestamp and offset information included
             */
            if (m.group(7) != null) {
                int ohh = Integer.parseInt(m.group(9));
                int omm = Integer.parseInt(m.group(10));

                /* time zone offset is specified in miliseconds */
                int offset = (ohh * 60 + omm) * 60 * 1000;

                if (m.group(8).equals("-")) {
                    offset = -offset;
                }

                TimeZone tz = TimeZone.getTimeZone("UTC");
                tz.setRawOffset(offset);

                builder.setTimeZone(tz);
            }

            mDate = builder.build().getTime();
        }
    }

    public ObexTime(Date date) {
        mDate = date;
    }

    public Date getTime() {
        return mDate;
    }

    @Override
    public String toString() {
        if (mDate == null) {
            return null;
        }

        Calendar cal = Calendar.getInstance();
        cal.setTime(mDate);

        /* note that months are numbered stating from 0 */
        return String.format(Locale.US, "%04d%02d%02dT%02d%02d%02d", cal.get(Calendar.YEAR),
                cal.get(Calendar.MONTH) + 1, cal.get(Calendar.DATE), cal.get(Calendar.HOUR_OF_DAY),
                cal.get(Calendar.MINUTE), cal.get(Calendar.SECOND));
    }
}
