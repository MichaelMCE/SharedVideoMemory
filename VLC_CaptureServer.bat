@echo off

SET VLCPATH=C:\Program Files (x86)\vlcStream32
SET WIDTH=960
SET HEIGHT=540


Start "" "%VLCPATH%\vlc.exe" --vout=svmem --svmem-width=%WIDTH% --svmem-height=%HEIGHT% --svmem-chroma=RV16 --goom-width=%WIDTH% --goom-height=%HEIGHT% --effect-width=%WIDTH% --effect-height=%HEIGHT% %1 

rem cmd /C call 


