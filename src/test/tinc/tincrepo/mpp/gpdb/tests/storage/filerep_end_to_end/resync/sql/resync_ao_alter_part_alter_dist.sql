-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- RESYNC AO TABLE 1
--
CREATE TABLE resync_ao_alter_part_alter_dist1 (id int, name text,rank int, year date, gender char(1))   with ( appendonly='true') DISTRIBUTED randomly
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into resync_ao_alter_part_alter_dist1 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_alter_dist1;


--
-- RESYNC AO TABLE 2
--
CREATE TABLE resync_ao_alter_part_alter_dist2 (id int, name text,rank int, year date, gender char(1))   with ( appendonly='true') DISTRIBUTED randomly
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into resync_ao_alter_part_alter_dist2 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_alter_dist2;


--
-- RESYNC AO TABLE 3
--
CREATE TABLE resync_ao_alter_part_alter_dist3 (id int, name text,rank int, year date, gender char(1))   with ( appendonly='true') DISTRIBUTED randomly
partition by list (gender)
subpartition by range (year)
subpartition template (
start (date '2001-01-01'))
(
values ('M'),
values ('F')
);
--
-- Insert few records into the table
--
insert into resync_ao_alter_part_alter_dist3 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_alter_dist3;



--
-- ALTER SYNC1 AO TABLE 
--
--
-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED BY
--
alter table sync1_ao_alter_part_alter_dist6 set distributed BY (id, gender, year);
--
-- Insert few records into the table
--

insert into sync1_ao_alter_part_alter_dist6 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_alter_dist6;


-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED RANDOMLY
--
alter table sync1_ao_alter_part_alter_dist6 set distributed randomly;
--
-- Insert few records into the table
--

insert into sync1_ao_alter_part_alter_dist6 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from sync1_ao_alter_part_alter_dist6;

--
-- ALTER CK_SYNC1 AO TABLE 
--
--
-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED BY
--
alter table ck_sync1_ao_alter_part_alter_dist5 set distributed BY (id, gender, year);
--
-- Insert few records into the table
--

insert into ck_sync1_ao_alter_part_alter_dist5 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_alter_dist5;


-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED RANDOMLY
--
alter table ck_sync1_ao_alter_part_alter_dist5 set distributed randomly;
--
-- Insert few records into the table
--

insert into ck_sync1_ao_alter_part_alter_dist5 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from ck_sync1_ao_alter_part_alter_dist5;

--
-- ALTER CT AO TABLE 
--
--
-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED BY
--
alter table ct_ao_alter_part_alter_dist3 set distributed BY (id, gender, year);
--
-- Insert few records into the table
--

insert into ct_ao_alter_part_alter_dist3 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_alter_dist3;


-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED RANDOMLY
--
alter table ct_ao_alter_part_alter_dist3 set distributed randomly;
--
-- Insert few records into the table
--

insert into ct_ao_alter_part_alter_dist3 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from ct_ao_alter_part_alter_dist3;

--
-- ALTER RESYNC AO TABLE 
--
--
-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED BY
--
alter table resync_ao_alter_part_alter_dist1 set distributed BY (id, gender, year);
--
-- Insert few records into the table
--

insert into resync_ao_alter_part_alter_dist1 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_alter_dist1;


-- ALTER PARTITION TABLE ALTER SET DISTRIBUTED RANDOMLY
--
alter table resync_ao_alter_part_alter_dist1 set distributed randomly;
--
-- Insert few records into the table
--

insert into resync_ao_alter_part_alter_dist1 values (generate_series(1,10),'ann',1,'2001-01-01','F');
--
-- select from the Table
--
select count(*) from resync_ao_alter_part_alter_dist1;


