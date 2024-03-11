del tmp.txt
del BlitHelperShaders.h

fxc /Tvs_5_1 /EVSMain /Vn g_VSMain BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPSBasic /Vn g_PSBasic BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPSBasic_SwapRB /Vn g_PSBasic_SwapRB BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPSAYUV /Vn g_PSAYUV BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPSY4XX /Vn g_PSY4XX BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPSPackedYUV /Vn g_PSPackedYUV BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPS2PlaneYUV /Vn g_PS2PlaneYUV BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPS3PlaneYUV /Vn g_PS3PlaneYUV BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h



fxc /Tps_5_1 /EPSBasic_ColorKeySrc /Vn g_PSBasic_ColorKeySrc BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPSBasic_SwapRB_ColorKeySrc /Vn g_PSBasic_SwapRB_ColorKeySrc BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPSAYUV_ColorKeySrc /Vn g_PSAYUV_ColorKeySrc BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPSY4XX_ColorKeySrc /Vn g_PSY4XX_ColorKeySrc BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPSPackedYUV_ColorKeySrc /Vn g_PSPackedYUV_ColorKeySrc BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPS2PlaneYUV_ColorKeySrc /Vn g_PS2PlaneYUV_ColorKeySrc BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

fxc /Tps_5_1 /EPS3PlaneYUV_ColorKeySrc /Vn g_PS3PlaneYUV_ColorKeySrc BlitHelperShaders.hlsl /Fh tmp.txt
type tmp.txt >> BlitHelperShaders.h

del tmp.txt