CREATE TABLE cr_ao_ctas WITH (appendonly=true) AS SELECT * FROM cr_seed_table_for_ao;
\d cr_ao_ctas
select count(*) from cr_ao_ctas;
drop table cr_ao_ctas;
drop table cr_seed_table_for_ao;
