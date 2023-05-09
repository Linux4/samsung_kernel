/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.bluetooth.btservice.storage;

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;

import java.util.List;

@Dao
interface MetadataDao {
    /**
     * Load all items in the database
     */
    @Query("SELECT * FROM metadata ORDER BY last_active_time DESC")
    List<Metadata> load();

    /**
     * Create or update a Metadata in the database
     */
    @Insert(onConflict = OnConflictStrategy.REPLACE)
    void insert(Metadata... metadata);

    /**
     * Delete a Metadata in the database
     */
    @Query("DELETE FROM metadata WHERE address = :address")
    void delete(String address);

    /**
     * Delete all Metadatas in the database
     */
    @Query("DELETE FROM metadata")
    void deleteAll();
}
