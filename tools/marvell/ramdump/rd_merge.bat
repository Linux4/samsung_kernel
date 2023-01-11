@set dr=%~d1%~p1
copy /B %dr%\ddr0_0x80000000--0x8fffffff.lst +%dr%\ddr1_0xc0000000--0xc7ffffff.lst %dr%\rd.bin
pause
