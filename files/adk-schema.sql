/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

DROP TABLE IF EXISTS `config_table`;
CREATE TABLE IF NOT EXISTS `config_table` (
	`key`	TEXT UNIQUE,
	`value`	TEXT,
	`value_default`	TEXT,
	`meta_id`	INTEGER,
	`read_only`	INTEGER,
	`internal_only`	INTEGER,
	FOREIGN KEY(`meta_id`) REFERENCES `meta_table`(`id`)
);

DROP TABLE IF EXISTS `meta_table`;
CREATE TABLE IF NOT EXISTS `meta_table` (
	`id`	INTEGER,
	`description`	TEXT,
	`value_type`	TEXT,
	`value_constraints`	TEXT,
	PRIMARY KEY(`id`)
);

DROP TABLE IF EXISTS `database_meta_table`;
CREATE TABLE IF NOT EXISTS `database_meta_table` (
	`key`	TEXT,
	`value`	TEXT
);

INSERT INTO `database_meta_table` VALUES ('database_version','1.0');
INSERT INTO config_table VALUES ("modeflow.volume.count", 14, 14, 2, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.0", 7, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.1", 7, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.2", 11, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.3", 11, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.4", 11, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.5", 11, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.6", 11, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.7", 11, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.8", 11, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.9", 11, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.10", 21, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.11", 7, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.12", 7, 7, 3, 0, 0);
INSERT INTO config_table VALUES ("modeflow.volume.13", 7, 7, 3, 0, 0);
INSERT INTO meta_table(id, value_type,value_constraints,description) VALUES (1, "UKNOWN",NULL,"Default constraints for UKNOWN types");
INSERT INTO meta_table(id, value_type, value_constraints, description) VALUES (2, "int",'{"range":[{"min":0}]}',"volume_list");
INSERT INTO meta_table(id, value_type, value_constraints, description) VALUES (3, "int",'{"range":[{"min":0, "max":22}]}',"volume");
