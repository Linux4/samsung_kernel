The use of reserved memory adaptive script

1.Location

kernel4.14/scripts/sprd/reset_reserved_memory.sh

2.Configuration file

location:kernel4.14/sprd-reserved-memory

Script supports deletion, modification and addition of reserved memory module
Keyword:DEL MOD ADD

Note: there must be no space between the keyword and the module. There should be one or
      more spaces (or tab) between the module and the size, size and starting address.
	  There must be a colon after the keyword

2.1 DEL

DEL:module_name

for example    DEL:tos

2.2 MOD

MOD:module_name size [start_address]

for example
MOD:wb 0x00584000
MOD:cp 0x0x02600000 0x89600000

2.3 ADD

ADD:module_name size

for example ADD:unisoc 0x00300000

3.Execute script

a.Make sure that you add what needs to be modified into the relevant
 configuration file according to the syntax in step 2.

b.Please enter any directory of kernel4.14 and running reset_ reserved_ memory.sh.
  ./scripts/sprd/reset_reserved_memory.sh

4. add board

location: kernel4.14/sprd-reserved-memory/add_board.sh

for example: add pike2_1h20

git diff
diff --git a/sprd-reserved-memory/add_board.sh b/sprd-reserved-memory/add_board.sh
index 885de9a..915c8966 100755
--- a/sprd-reserved-memory/add_board.sh
+++ b/sprd-reserved-memory/add_board.sh
@@ -2,6 +2,7 @@ PROJECT_ARM_ARRAY=(
 sp7731e-1h10
 sp9832e-1h10_go
 sp9863a-1h10_go_32b
+sp7731e-1h20
 )
