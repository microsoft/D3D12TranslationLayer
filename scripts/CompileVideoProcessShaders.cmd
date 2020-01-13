del tmp.txt
del VideoProcessShaders.h

fxc /Tvs_5_1 /EVSMain /Vn g_DeinterlaceVS DeinterlaceShader.hlsl /Fh tmp.txt
type tmp.txt >> VideoProcessShaders.h

fxc /Tps_5_1 /EPSMain /Vn g_DeinterlacePS DeinterlaceShader.hlsl /Fh tmp.txt
type tmp.txt >> VideoProcessShaders.h

del tmp.txt