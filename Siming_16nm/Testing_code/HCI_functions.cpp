/******************************
Functions eNVM
*******************************/
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "StdAfx.h"
#include "NIDAQmx.h"
#include <stdlib.h>
#include <windows.h>
#include "ni488.h"
#include <stdio.h>
#include <string>
#include <visa.h>
#include <time.h>
#include "TestFunctions.h"


/*********************ADAPTED from stress_VG_ConstPulse************************/
int erase_VSDB_tunneling(char* Measure_file, double VSDB, double VDD_DIG_WL, char* pulse_width_char, int chip, int col, int direction, int Num_of_Pulse){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];
	char erase_MUX_Address_file_stress[200], erase_MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(erase_MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain_erase", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(erase_MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource_erase", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(erase_MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource_erase", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(erase_MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain_erase", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, 1);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	/*	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0); */

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-ExtTrigger-ed measurements within a WLpulse
	//float NPLCycles = 1.0;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001]; //for multi-trigger/sample, innitiate->fetch data retrival

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0); //Need to update!!!
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, 1);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	for (row = 0; row < Num_of_row[col]; row++){
		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		for (t = 1; t <= Num_of_Pulse; t++){

			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			//DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
			DO_USB6008("../Scan_files/MUX_OFF_erase"); //all mux disabled, POLARITY[1:2]=1
			//DO_USB6008(MUX_Address_file_stress);
			DO_USB6008(erase_MUX_Address_file_stress);

			//scan("../Scan_files/MUX_ON", 0, 100000.0);

			//multiple triggers, single sample
			MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, 1);
			//char RdBuffer[12001];

			_ibwrt(_MM34401A, "INITiate");
			_ibwrt(_MM34410A_6, "INITiate");

			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_DIG_WL); //VDD_DIG=VDD_WL=VDD_DIG_WL, avoid any un-intentional crowbar current or turn-on diodes
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_DIG_WL); //VDD_WL=VDD_DIG_WL, for unselected cells during erase, and also biasing the DNW properly to avoid the diode between DNW and VSS_PW from turning on!!!
			E3646A_SetVoltage(_VSPARE_VAB, 2, VSDB);     //Vdrain = VSDB
			E3646A_SetVoltage(_VSPARE_VAB, 1, VSDB);     //Vsource = VSDB
			E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VSDB);    //Finally, VSS_PW=VSDB, 12mA limit
			//ATTENTION: the order of these voltages, avoid turning on diodes!
			::Sleep(30);

			// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
			// before toggling PULSE and trigger Isub measurement
			//multiple triggers, single sample
			scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

			E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);    //First, VSS_PW resume to 0
			::Sleep(70);
			//VSS_PW=0, we can lower all VG=0 now.
			DO_USB6008(MUX_Address_file_stress); //MUX keep connections while POLARITY[1:2]=0 to turn off all WL=VG=0
			//DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, all WL=0
			//we need to do this before lowering Vsource and Vdrain to avoid HCI injection again
			//because the asynchronous change of Vsource and Vdrain
			//while MUX is still connecting, lower Vdrain and Vsource to 0, to avoid them from floating before being grounded
			E3646A_SetVoltage(_VSPARE_VAB, 1, 0);       //Vsource = 0
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0);       //Vdrain = 0
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0

			_ibwrt(_MM34401A, "FETCh?");
			_ibrd(_MM34401A, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < 1; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Erase_%02dPULSE_WL[%d]_ID=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			_ibwrt(_MM34410A_6, "FETCh?");
			_ibrd(_MM34410A_6, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < 1; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Erase_%02dPULSE_WL[%d]_Isub=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
			/*E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);*/
			DO_USB6008(MUX_Address_file_stress);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);

			DO_USB6008(MUX_Address_file_mirror);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
		}

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\nRetention: short-term recovery\n", lt.wHour, lt.wMinute, lt.wSecond);
	::Sleep(300000);
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	fclose(f_ptr);
	return 0;
}

/*********************ADAPTED from Siming_28nm************************/
int ALL_IDSAT(char* Measure_file, int chip, int col, int direction){

	/******Need to double check and debug the MUX operations*******/
	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	FILE *f_ptr;
	//	if ((f_ptr = fopen(Measure_file, "w")) == NULL){
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){ //open file to append
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	int row;
	float Isense;

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0); //need to update!
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "ALL_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "ALL_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}

	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);

	scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	fclose(f_ptr);
	return 0;
}

/*********************Adapted from Block_GateErase****************/
/**** It's WORKING! Except for some write data file format issues... Can make do for now... ********/
int Block_FN_tunnel(char* Measure_file, double VDD_DIG, double VSS_WL, double VDD_WL, char* pulse_width_char, int Num_of_ExtTrig, int chip, int col, int direction){

	ALL_IDSAT(Measure_file, chip, col, 0);
//	ALL_IDSAT(Measure_file, chip, col, 1);

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];  
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, Num_of_ExtTrig);

	// scan in WL[0]=1 in column[col], pulse=0
	/*	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0); */

	//	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	// scan in all WL[0:31]=1 in column[20], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_allWL_NOpulse", col); //scan in 1 for all the 32 cells of col[20]
	scan(f_scan, 0, 100000.0);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){ //open file to append
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-EstTrigger-ed measurements within a WLpulse
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001];


	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VAB = VD = 0V
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); // VSPARE = VS = 0V
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0); //VSS_PW = 0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	DO_USB6008(MUX_Address_file_stress);

	//scan("../Scan_files/MUX_ON", 0, 100000.0);

	//multiple triggers, single sample
	MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig);

	//multiple triggers, single sample
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig);

	//multiple triggers, single sample
	MM34401A_MeasCurrent_Config(_MM34401A_7, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	_ibwrt(_MM34401A, "INITiate");
	_ibwrt(_MM34410A_6, "INITiate");
	_ibwrt(_MM34401A_7, "INITiate");


	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_DIG);
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, VSS_WL);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL);

	// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
	// before toggling PULSE and trigger Isub measurment
	//multiple triggers, single sample
	scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical);

	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);

	_ibwrt(_MM34401A, "FETCh?");
	_ibrd(_MM34401A, RdBuffer, 12000);
	RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
	//printf("%s\n", RdBuffer);

	for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "FN_Tunnel_ID=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
	}

	_ibwrt(_MM34410A_6, "FETCh?");
	_ibrd(_MM34410A_6, RdBuffer, 12000);
	RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
	//printf("%s\n", RdBuffer);

	for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "FN_Tunnel_Isub=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
	}

	_ibwrt(_MM34401A_7, "FETCh?");
	_ibrd(_MM34401A_7, RdBuffer, 12000);
	RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
	//printf("%s\n", RdBuffer);

	for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "FN_Tunnel_IS=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
	}

	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
	ALL_IDSAT(Measure_file, chip, col, 0);
//	ALL_IDSAT(Measure_file, chip, col, 1);

	if ((f_ptr = fopen(Measure_file, "a")) == NULL){ //open file to append
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	::Sleep(60000);

	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
	ALL_IDSAT(Measure_file, chip, col, 0);
//	ALL_IDSAT(Measure_file, chip, col, 1);

	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	fclose(f_ptr);

	return 0;
}

/******************************ADAPTED from Block_Erase********************************/
/*************** PULSE+DMM measurements NOT WORKING******************************/
int Block_GateErase(char* Measure_file, double VDD_DIG, double VSS_WL, double VDD_WL, char* pulse_width_char, int Num_of_ExtTrig, int chip, int col, int direction){

	ALL_IDSAT(Measure_file, chip, col, 0);

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];  //1min WL pulse: 27440ms + 5120ms (512external triggers) + 27440ms
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, Num_of_ExtTrig);
	//sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sNO_PULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, Num_of_ExtTrig);
	//got rid of the PULSE just in case (even though shouldn't matter since scanned all zero).

	// scan in WL[0]=1 in column[col], pulse=0
	/*	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0); */

	//	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	// scan in all WL[0:31]=1 in column[20], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_allWL_NOpulse", col); //scan in 1 for all the 32 cells of col[20]
	scan(f_scan, 0, 100000.0);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){ //open file to append
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-EstTrigger-ed measurements within a WLpulse
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001];


	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	DO_USB6008(MUX_Address_file_stress);

	//scan("../Scan_files/MUX_ON", 0, 100000.0);

	//multiple triggers, single sample
	MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig);

	//multiple triggers, single sample
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	_ibwrt(_MM34401A, "INITiate");
	_ibwrt(_MM34410A_6, "INITiate");


	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_DIG);
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, VSS_WL);
	::Sleep(30);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL);

	// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
	// before toggling PULSE and trigger Isub measurment
	//multiple triggers, single sample
	scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, 0.8);
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, 0.8);

	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);

	_ibwrt(_MM34401A, "FETCh?");
	_ibrd(_MM34401A, RdBuffer, 12000);
	RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
	//printf("%s\n", RdBuffer);

	for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "Erase_I_VD=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
	}

	_ibwrt(_MM34410A_6, "FETCh?");
	_ibrd(_MM34410A_6, RdBuffer, 12000);
	RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
	//printf("%s\n", RdBuffer);

	for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "Erase_Isub=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
	}

	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	fclose(f_ptr);

	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
	ALL_IDSAT(Measure_file, chip, col, 0);
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	::Sleep(60000);

	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
	ALL_IDSAT(Measure_file, chip, col, 0);

	return 0;
}

/******************************ADAPTED from Siming_28nm********************************/
int Block_Erase(char* Measure_file, double VD, double VB, double VS, double VDD_DIG, double VSS_WL, double VDD_WL, char* pulse_scan_file, int Num_of_ExtTrig, int chip, int col, int direction, int Erase_Cycle){

/**************	at 20C **************/
	ALL_IDSAT(Measure_file, chip, col, 0);

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%s", pulse_scan_file);
	//got rid of the PULSE just in case (even though shouldn't matter since scanned all zero).

	// scan in all WL's=1 in 2FIN domain, pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_2FIN_allWL_NOpulse");
	scan(f_scan, 0, 100000.0); 
	// CAUTIOUS: all WL's of the 2nd half of the chip "2FIN" domain VG=VDD_WL during Block_Erase
	
	/*
	// scan in all WL's=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_allWL_NOpulse", col);
	scan(f_scan, 0, 100000.0); 
	// CAUTIOUS: all WL's of the selected column VG=VDD_WL during Block_Erase
	*/
	
	//Rather than all VG=VSS_WL
	//scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	FILE *f_ptr;

	for (int cycle = 0; cycle < Erase_Cycle; cycle++){

/**************	Heat up the chamber to 125C **************/

		if ((f_ptr = fopen(Measure_file, "a")) == NULL){ //open file to append
			printf("Cannot open%s.\n", Measure_file);
			return FAIL;
		}

		SYSTEMTIME lt;
		GetLocalTime(&lt);
		fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
		fprintf(f_ptr, "Scan_file: %s, effective number of triggers: the initial %d\n", pulse_scan_file, Num_of_ExtTrig);

		//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
		int row, t, j;
		float NPLCycles = 10.0;  //integration time for Multi-EstTrigger-ed measurements within a WLpulse
		float Isense;
		//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
		float Current[512];
		//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
		char StrCurrent[16];
		char RdBuffer[12001];


		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VD = VAB = 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
		DO_USB6008(MUX_Address_file_stress);

		//multiple triggers, single sample
		MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig);

		//multiple triggers, single sample
		MM34401A_MeasCurrent_Config(_MM34401A_7, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig); //IS

		//multiple triggers, single sample
		MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig);


		_ibwrt(_MM34401A, "INITiate");
		_ibwrt(_MM34401A_7, "INITiate");  //IS
		_ibwrt(_MM34410A_6, "INITiate"); 

		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_DIG); 
		E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, VSS_WL);    
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL); 
		E3646A_SetVoltage(_VSPARE_VAB, 1, VS);     //VSPARE = VS: first raise the source
		E3646A_SetVoltage(_VSPARE_VAB, 2, VD);     //VAB = VD: then raise the drain
		E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VB);  //VSS_PW = VB: finally raise the PW

		::Sleep(30);

		// 30ms initial delay to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
		// before toggling PULSE and trigger Isub measurment
		//multiple triggers, single sample
		long_scan(f_scan_WLpulse_ExtTrig, samp_rate);
	//	scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);
		//For some reason, the scan only lasted for about ~20sec!
		/*temporary solution*/
		//::Sleep(60000); //wait for Ijunction to flow for 1min
		/*temporary solution*/

		E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);  //first lower the PW 
		::Sleep(70);
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);     //then lower the drain
		E3646A_SetVoltage(_VSPARE_VAB, 1, 0);     //finally lower the source
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);    //VSS_WL=0
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);

		_ibwrt(_MM34401A, "FETCh?");
		_ibrd(_MM34401A, RdBuffer, 12000);
		RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string
		//printf("%s\n", RdBuffer);

		for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "Erase_I_VD=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
		}


		_ibwrt(_MM34401A_7, "FETCh?");
		_ibrd(_MM34401A_7, RdBuffer, 12000);
		RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string
		//printf("%s\n", RdBuffer);

		for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "Erase_I_VS=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
		}


		_ibwrt(_MM34410A_6, "FETCh?");
		_ibrd(_MM34410A_6, RdBuffer, 12000);
		RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string
		//printf("%s\n", RdBuffer);

		for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "Erase_Isub=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
		}

		GetLocalTime(&lt);
		fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
		fclose(f_ptr);

/**************	cool down the chamber to 25C **************/

		ALL_IDSAT(Measure_file, chip, col, 0);
	}

	return 0;
}

/******************************ADAPTED from Charge_Pumping ********************************/
int Charge_Pumping_ELTM(char* Measure_file, double* VDBS_list_Vr0, int Num_of_VDBS, double VDD_DIG, double VSS_WL, double VDD_WL, char* scan_file, double samp_rate, int Num_of_freq, double* pumping_freq, int Num_of_ExtTrig, int chip, int col, int direction){
	// Charge Pumping measurement using electrometer

	//	ALL_IDSAT(Measure_file, chip, col, 0);

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

	int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void *callbackData);
	//This function is defined in the counter output example code

	// Variables for Error Buffer
	int32 error = 0;
	char errBuff[2048] = { '\0' };

	//NIDAQ ANSI C example code, counter/digital pulse train continuous generation
	//we use counter1 for WL pulses ONLY during charge pumping measurment
	TaskHandle  taskHandleCounter1 = 0;
//	DAQmxErrChk(DAQmxCreateTask("", &taskHandleCounter1));


	char f_scan_WLpulse_ExtTrig[200];
	//	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, Num_of_ExtTrig);
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%s", scan_file); 
	// This scan file only provides ExtTrig for both DC(no pumping) and pumping measurements;
	// WL pulses during pumping are generated from and temperarily RE-CONNECTED to NIDAQ counter1 !!!

	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	// scan in 1 for all WLs in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_allWL_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	FILE *f_ptr;

	if ((f_ptr = fopen(Measure_file, "a")) == NULL){ //open file to append
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	//j: number of ExtTrig measurement
	int j;
	//	float NPLCycles = 100;  //integration time (1.67s + AutoZero) for Multi-ExtTrigger-ed measurements
	float NPLCycles = 10; //integration time (167ms + AutoZero) for Multi-ExtTrigger-ed measurements
	float Isense;
	//the ELTM's internal memory can hold ? readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDSEDD,SD.DDDDDDSEDD,SD.DDDDDDSEDD,....", so 13+1(comma) = 14 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001];

	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VD = VAB = 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	DO_USB6008(MUX_Address_file_stress);
	//scan("../Scan_files/MUX_ON", 0, 100000.0);

	for (int v = 0; v < Num_of_VDBS; v++){
		double VDBS = VDBS_list_Vr0[v];
		fprintf(f_ptr, "VS=%f, VD=%f, VB=%f\n", VDBS, VDBS, VDBS);

		//Source (MM34401A_7) and Drain (MM34401A) DMMs are not connected into the circuit in this charge pumping measurement
		//multiple triggers, single sample
		//	MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", Trig_Delay, Num_of_Sample, Num_of_ExtTrig);

		//multiple triggers, single sample
		//	MM34401A_MeasCurrent_Config(_MM34401A_7, NPLCycles, "EXT", Trig_Delay, Num_of_Sample, Num_of_ExtTrig); //IS

		//multiple triggers, single sample

//		ELTM6514_MeasCurrent_Config(_ELTM6514, Num_of_ExtTrig, NPLCycles);

		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_DIG);
		E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, VSS_WL);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL); //VDD_WL>=VSS_PW-Vdiode(DNW-PW)
		E3646A_SetVoltage(_VSPARE_VAB, 1, VDBS);     //VSPARE = VS: first raise the source
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDBS);     //VAB = VD: then raise the drain
		E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VDBS);  //VSS_PW = VB: finally raise the PW

		SYSTEMTIME lt;
/*		DAQmxErrChk(DAQmxCreateTask("", &taskHandleCounter1));
		//The output frequency is set arbitraritly, counter idle state = high
		DAQmxErrChk(DAQmxCreateCOPulseChanFreq(taskHandleCounter1, "Dev1/ctr1", "", DAQmx_Val_Hz, DAQmx_Val_High, 0.0, 10000, 0.50));
		//continuous output, specify a large enough buffer size
		DAQmxErrChk(DAQmxCfgImplicitTiming(taskHandleCounter1, DAQmx_Val_ContSamps, 1000));
		DAQmxErrChk(DAQmxRegisterDoneEvent(taskHandleCounter1, 0, DoneCallback, NULL));

		/*********************************************/
		// start followed by stop: only to make the idle state of the counter output take effect
		// WL pulse input PAD change to high
		/*********************************************/
/*		DAQmxErrChk(DAQmxStartTask(taskHandleCounter1));
		::Sleep(1000);
		DAQmxStopTask(taskHandleCounter1);
		DAQmxClearTask(taskHandleCounter1);


		::Sleep(600000); //10min (~?10RC -> 99.99%?) for TESTING!

		_ibwrt(_ELTM6514, "INITiate");
		long_scan(f_scan_WLpulse_ExtTrig, samp_rate);  //scan frequency is samp_rate = 10Hz (0.1s)
		// 60 (0.1s width) ExtTrigs, 1s intervals
		// ELTM measure substrate current without pumping
		SYSTEMTIME lt;
		GetLocalTime(&lt);
		fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
		fprintf(f_ptr, "No pumping, counter idle high, 10min test settling\n");

		_ibwrt(_ELTM6514, "FETCh?");
		_ibrd(_ELTM6514, RdBuffer, 12000);
		RdBuffer[ibcntl] = '\0';
		printf("%s\n", RdBuffer);
		for (j = 0; j < Num_of_ExtTrig; j++){
			//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
			//each reading has 13 charaters, plus one comma
			strncpy(StrCurrent, RdBuffer + j * 14, 13);
			StrCurrent[13] = '\0';
			sscanf(StrCurrent, "%f", &Current[j]);

			fprintf(f_ptr, "ELTM Isub_without_pumping_10min_TESTING_CounterIdleHigh=%.15f\n", Current[j]);
		}

		fprintf(f_ptr, "\n");

		//ARMed once, immediately -> multiple triggers, single sample
		ELTM6514_MeasCurrent_Config(_ELTM6514, Num_of_ExtTrig, NPLCycles);

		//::Sleep(1440000); //24min (~?10RC -> 99.99%?) settling time for VSS_PW current to reach a stable value
		::Sleep(840000); //another 14min

		_ibwrt(_ELTM6514, "INITiate");
		long_scan(f_scan_WLpulse_ExtTrig, samp_rate);  //scan frequency is samp_rate = 10Hz (0.1s)
		// 60 (0.1s width) ExtTrigs, 1s intervals
		// ELTM measure substrate current without pumping

		GetLocalTime(&lt);
		fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
		fprintf(f_ptr, "No pumping, counter idle high, another 14min settling\n");

		_ibwrt(_ELTM6514, "FETCh?");
		_ibrd(_ELTM6514, RdBuffer, 12000);
		RdBuffer[ibcntl] = '\0';
		printf("%s\n", RdBuffer);
		for (j = 0; j < Num_of_ExtTrig; j++){
			//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
			//each reading has 13 charaters, plus one comma
			strncpy(StrCurrent, RdBuffer + j * 14, 13);
			StrCurrent[13] = '\0';
			sscanf(StrCurrent, "%f", &Current[j]);

			fprintf(f_ptr, "ELTM Isub_without_pumping_25min_CounterIdleHigh=%.15f\n", Current[j]);
		}

		fprintf(f_ptr, "\n");

		DAQmxErrChk(DAQmxCreateTask("", &taskHandleCounter1));
		//The output frequency is set arbitraritly, counter idle state = low
		DAQmxErrChk(DAQmxCreateCOPulseChanFreq(taskHandleCounter1, "Dev1/ctr1", "", DAQmx_Val_Hz, DAQmx_Val_Low, 0.0, 10000, 0.50));
		//continuous output, specify a large enough buffer size
		DAQmxErrChk(DAQmxCfgImplicitTiming(taskHandleCounter1, DAQmx_Val_ContSamps, 1000));
		DAQmxErrChk(DAQmxRegisterDoneEvent(taskHandleCounter1, 0, DoneCallback, NULL));

		/*********************************************/
		// start followed by stop: only to make the idle state of the counter output take effect
		// WL pulse input PAD change to LOW
		/*********************************************/
/*		DAQmxErrChk(DAQmxStartTask(taskHandleCounter1));
		::Sleep(1000);
		DAQmxStopTask(taskHandleCounter1);
		DAQmxClearTask(taskHandleCounter1);

		//ARMed once, immediately -> multiple triggers, single sample
		ELTM6514_MeasCurrent_Config(_ELTM6514, Num_of_ExtTrig, NPLCycles);

		::Sleep(600000); //10min (~?10RC -> 99.99%?) for TESTING!

		_ibwrt(_ELTM6514, "INITiate");
		long_scan(f_scan_WLpulse_ExtTrig, samp_rate);  //scan frequency is samp_rate = 10Hz (0.1s)
		// 60 (0.1s width) ExtTrigs, 1s intervals
		// ELTM measure substrate current without pumping

		GetLocalTime(&lt);
		fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
		fprintf(f_ptr, "No pumping, counter idle low, 10min test settling\n");

		_ibwrt(_ELTM6514, "FETCh?");
		_ibrd(_ELTM6514, RdBuffer, 12000);
		RdBuffer[ibcntl] = '\0';
		printf("%s\n", RdBuffer);
		for (j = 0; j < Num_of_ExtTrig; j++){
			//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
			//each reading has 13 charaters, plus one comma
			strncpy(StrCurrent, RdBuffer + j * 14, 13);
			StrCurrent[13] = '\0';
			sscanf(StrCurrent, "%f", &Current[j]);

			fprintf(f_ptr, "ELTM Isub_without_pumping_10min_TESTING_CounterIdleLow=%.15f\n", Current[j]);
		}

		fprintf(f_ptr, "\n");

		//ARMed once, immediately -> multiple triggers, single sample
		ELTM6514_MeasCurrent_Config(_ELTM6514, Num_of_ExtTrig, NPLCycles);

		//::Sleep(1440000); //24min (~?10RC -> 99.99%?) settling time for VSS_PW current to reach a stable value
		::Sleep(840000); //another 14min

		_ibwrt(_ELTM6514, "INITiate");
		long_scan(f_scan_WLpulse_ExtTrig, samp_rate);  //scan frequency is samp_rate = 10Hz (0.1s)

		GetLocalTime(&lt);
		fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
		fprintf(f_ptr, "No pumping, Counter Idle Low, another 14min\n");

		_ibwrt(_ELTM6514, "FETCh?");
		_ibrd(_ELTM6514, RdBuffer, 12000);
		RdBuffer[ibcntl] = '\0';
		printf("%s\n", RdBuffer);
		for (j = 0; j < Num_of_ExtTrig; j++){
			//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
			//each reading has 13 charaters, plus one comma
			strncpy(StrCurrent, RdBuffer + j * 14, 13);
			StrCurrent[13] = '\0';
			sscanf(StrCurrent, "%f", &Current[j]);

			fprintf(f_ptr, "ELTM Isub_without_pumping=%.15f\n", Current[j]);
		}

		fprintf(f_ptr, "\n");
*/
		for (int f = 0; f < Num_of_freq; f++){
			double counter1_freq = pumping_freq[f];
			ELTM6514_MeasCurrent_Config(_ELTM6514, Num_of_ExtTrig, NPLCycles, "0.0000002");
/*			if (VDBS < 1.15){
				ELTM6514_MeasCurrent_Config(_ELTM6514, Num_of_ExtTrig, NPLCycles, "0.0000002");
			}
			if (VDBS > 1.15){
				ELTM6514_MeasCurrent_Config(_ELTM6514, Num_of_ExtTrig, NPLCycles, "0.000002");
			} */

			DAQmxErrChk(DAQmxCreateTask("", &taskHandleCounter1));
			//The output frequency is pumping_freq[f]
			DAQmxErrChk(DAQmxCreateCOPulseChanFreq(taskHandleCounter1, "Dev1/ctr1", "", DAQmx_Val_Hz, DAQmx_Val_Low, 0.0, counter1_freq, 0.50));
			//continuous output, specify a large enough buffer size
			DAQmxErrChk(DAQmxCfgImplicitTiming(taskHandleCounter1, DAQmx_Val_ContSamps, 1000));
			DAQmxErrChk(DAQmxRegisterDoneEvent(taskHandleCounter1, 0, DoneCallback, NULL));

			/*********************************************/
			// DAQmx Start Code
			/*********************************************/
			DAQmxErrChk(DAQmxStartTask(taskHandleCounter1));

			if (f == 0){
				::Sleep(300000); //If this is the 1st pumping frequency, need an extra 5min settling time (total = 5+7 = 12min)
				//since I no longer measure Isub without pumping, either counter idle low or high.

				/******not settled! after putting the PCB into the chamber(even not powered on!)********/
//				::Sleep(180000); // Add another 3min to improve settling
				/******not settled! after putting the PCB into the chamber(even not powered on!)********/
			}

			//Starting from the same settling states as the no pumping measurement
			//After turnning on counter 1 to pump all WLs
			//Give it another 4min settling time for VSS_PW current to reach a stable DC value
			//::Sleep(240000);
			::Sleep(420000); // Try 7min, to see whether there'd be further change compared with 4min

			/******not settled! after putting the PCB into the chamber(even not powered on!)********/
//			::Sleep(360000); // Add another 6min to improve settling
			/******not settled! after putting the PCB into the chamber(even not powered on!)********/

			_ibwrt(_ELTM6514, "INITiate");
			long_scan(f_scan_WLpulse_ExtTrig, samp_rate);
			// 60 (0.1s width) ExtTrigs, 1s intervals
			// ELTM measure substrate current with pumping

			DAQmxStopTask(taskHandleCounter1);
			DAQmxClearTask(taskHandleCounter1);

			GetLocalTime(&lt);
			fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);
			fprintf(f_ptr, "Charge pumping frequency = %f\n", counter1_freq);

			_ibwrt(_ELTM6514, "FETCh?");
			_ibrd(_ELTM6514, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';
			printf("%s\n", RdBuffer);
			for (j = 0; j < Num_of_ExtTrig; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
				//each reading has 13 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 14, 13);
				StrCurrent[13] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);

				fprintf(f_ptr, "ELTM Isub_with_pumping=%.15f\n", Current[j]);
			}

			fprintf(f_ptr, "\n");
		}
	}
	// now we can turn off these voltages
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);  //first lower the PW 
	::Sleep(70);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0);     //then lower the drain
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0);     //finally lower the source
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);

	fclose(f_ptr);

Error:
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);

	// I added this for counter out
	if (taskHandleCounter1 != 0){
		DAQmxStopTask(taskHandleCounter1);
		DAQmxClearTask(taskHandleCounter1);
	}
	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);

	return 0;
}

/******************************ADAPTED from Block_Erase********************************/
int Charge_Pumping(char* Measure_file, double VD, double VB, double VS, double VDD_DIG, double VSS_WL, double VDD_WL, char* scan_file, double samp_rate, int Num_of_ExtTrig, int Num_of_Sample, double Trig_Delay, int chip, int col, int direction, int Isub_Rsense){

//	ALL_IDSAT(Measure_file, chip, col, 0);

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	char f_scan_WLpulse_ExtTrig[200];
	//	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, Num_of_ExtTrig);
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%s", scan_file);

	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	// scan in 1 for all WLs in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_allWL_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	FILE *f_ptr;

	if ((f_ptr = fopen(Measure_file, "a")) == NULL){ //open file to append
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	//j: number of ExtTrig measurement
	int j;
//	float NPLCycles = 100;  //integration time (1.67s + AutoZero) for Multi-ExtTrigger-ed measurements
	float NPLCycles = 10;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001];

	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VD = VAB = 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	DO_USB6008(MUX_Address_file_stress);
	//scan("../Scan_files/MUX_ON", 0, 100000.0);

	fprintf(f_ptr, "VS=%f, VD=%f, VB=%f\n", VS, VD, VB);

	//multiple triggers, single sample
//	MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", Trig_Delay, Num_of_Sample, Num_of_ExtTrig);

	//multiple triggers, single sample
//	MM34401A_MeasCurrent_Config(_MM34401A_7, NPLCycles, "EXT", Trig_Delay, Num_of_Sample, Num_of_ExtTrig); //IS

	//multiple triggers, single sample
	if (Isub_Rsense == 0){
		MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", Trig_Delay, Num_of_Sample, Num_of_ExtTrig);
	}
	if (Isub_Rsense == 1){ //Isub_Rsense=1, use a R(shunt)=100kOhm between VDD_PW[i] and the PSU output, and use DMM voltage(100mV range) to measure the voltage across it.
		MM34410A_6_MeasVoltage_Config(_MM34410A_6, NPLCycles, "EXT", Trig_Delay, Num_of_Sample, Num_of_ExtTrig);
	}
	if (Isub_Rsense == 2){
		//ELTM6514_MeasCurrent_Config(_ELTM6514, Num_of_ExtTrig, NPLCycles);
		ELTM6514_MeasCurrent_Config(_ELTM6514, Num_of_ExtTrig, NPLCycles, "0.0000002");
	}

/*	_ibwrt(_MM34401A, "INITiate");
	_ibwrt(_MM34401A_7, "INITiate");  //IS 
	if (Isub_Rsense == 2){
		_ibwrt(_ELTM6514, "INITiate");
	}
	else{
	    _ibwrt(_MM34410A_6, "INITiate"); 
	}*/

	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_DIG);
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, VSS_WL);    
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL); //VDD_WL>=VSS_PW-Vdiode(DNW-PW)
	E3646A_SetVoltage(_VSPARE_VAB, 1, VS);     //VSPARE = VS: first raise the source
	E3646A_SetVoltage(_VSPARE_VAB, 2, VD);     //VAB = VD: then raise the drain
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VB);  //VSS_PW = VB: finally raise the PW

	::Sleep(1000);
	::Sleep(60000); //1min settling time for VSS_PW current to reach a DC stable value
	//(global) VSS_PW needs to charge up all the floating VS/VD in unselected columns! Huge capacitance and time constant!

	// 1sec initial delay to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
	// before toggling PULSE and trigger Isub measurment
	// multiple triggers (1 for DC, 1 for pumping), single sample (can repeat this for multiple times to average!)
	// (100PLC=1.67s + other stuff => ExtTrig 5sec interval, sufficiently spaced apart)
//	long_scan(f_scan_WLpulse_ExtTrig, samp_rate);

	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);  //first lower the PW 
	::Sleep(70);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0);     //then lower the drain
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0);     //finally lower the source
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	_ibwrt(_MM34401A, "FETCh?");
	_ibrd(_MM34401A, RdBuffer, 12000);
	RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string
	//printf("%s\n", RdBuffer);

	for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "I_VD=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
	}


	_ibwrt(_MM34401A_7, "FETCh?");
	_ibrd(_MM34401A_7, RdBuffer, 12000);
	RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string
	//printf("%s\n", RdBuffer);

	for (j = 0; j < Num_of_ExtTrig; j++){
		//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
		//each reading has 15 charaters, plus one comma
		strncpy(StrCurrent, RdBuffer + j * 16, 15);
		StrCurrent[15] = '\0';
		sscanf(StrCurrent, "%f", &Current[j]);
		fprintf(f_ptr, "I_VS=%.12f\n", Current[j]);
		//debug:
		//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
	}
    
	if(Isub_Rsense == 2){
		_ibwrt(_ELTM6514, "FETCh?");
		_ibrd(_ELTM6514, RdBuffer, 12000);
		RdBuffer[ibcntl] = '\0';
		printf("%s\n", RdBuffer);
		for (j = 0; j < Num_of_ExtTrig; j++){
			//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
			//each reading has 13 charaters, plus one comma
			strncpy(StrCurrent, RdBuffer + j * 14, 13);
			StrCurrent[13] = '\0';
			sscanf(StrCurrent, "%f", &Current[j]);

			fprintf(f_ptr, "ELTM Isub=%.12f\n", Current[j]);
		}
	}
	else{
		_ibwrt(_MM34410A_6, "FETCh?");
		_ibrd(_MM34410A_6, RdBuffer, 12000);
		RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string
		//printf("%s\n", RdBuffer);

		for (j = 0; j < Num_of_ExtTrig; j++){
			//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory
			//each reading has 15 charaters, plus one comma
			strncpy(StrCurrent, RdBuffer + j * 16, 15);
			StrCurrent[15] = '\0';
			sscanf(StrCurrent, "%f", &Current[j]);
			if (Isub_Rsense == 0){
				fprintf(f_ptr, "Isub=%.12f\n", Current[j]);
			}
			if (Isub_Rsense == 1){
				fprintf(f_ptr, "Vsub=%.12f\n", Current[j]);
			}
		}
	}
	fprintf(f_ptr, "\n");

	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	fclose(f_ptr);

	return 0;
}

int Erase_VG_ConstPulse(char* Measure_file, double VDS, double VGS, char* pulse_width_char, int chip, int col, int direction, int Num_of_Pulse, int Num_of_Trigger){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, Num_of_Trigger);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-ExtTrigger-ed measurements within a WLpulse
	//float NPLCycles = 1.0;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001]; //for multi-trigger/sample, innitiate->fetch data retrival

	/*	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	for (row = 0; row < Num_of_row[col]; row++){
	//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	fprintf(f_ptr, "WL[%d]\n", row);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008(MUX_Address_file_stress);
	scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
	Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);

	fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
	//debug: TODO these printf's waste resource/time???
	//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

	DO_USB6008(MUX_Address_file_mirror);
	scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
	Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);

	fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
	//debug:
	//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

	// shift down one row
	scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0); */
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, Num_of_Trigger);

	for (row = 0; row < Num_of_row[col]; row++){
		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "PreErase_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "PreErase_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		for (t = 1; t <= Num_of_Pulse; t++){

			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008(MUX_Address_file_stress);

			scan("../Scan_files/MUX_ON", 0, 100000.0);

			//multiple triggers, single sample
			MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, Num_of_Trigger);
			//char RdBuffer[12001];

			_ibwrt(_MM34401A, "INITiate");
			_ibwrt(_MM34410A_6, "INITiate");

			/*			E3646A_SetVoltage(_VSPARE_VAB, 2, VDS[row]);     //VA=VDS
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS[row]); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS[row]); //VDD_WL=VGS */

			E3646A_SetVoltage(_VSPARE_VAB, 2, VDS);     //VA=VDS
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS); //VDD_WL=VGS

			// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
			// before toggling PULSE and trigger Isub measurment
			//multiple triggers, single sample
			scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
			scan("../Scan_files/MUX_OFF", 0, 100000.0);

			_ibwrt(_MM34401A, "FETCh?");
			_ibrd(_MM34401A, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < Num_of_Trigger; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Erase_%02dPULSE_WL[%d]_ID_program=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			_ibwrt(_MM34410A_6, "FETCh?");
			_ibrd(_MM34410A_6, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < Num_of_Trigger; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Erase_%02dPULSE_WL[%d]_Isub=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
			/*	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008(MUX_Address_file_stress); */
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);

			fprintf(f_ptr, "Erase_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);

			DO_USB6008(MUX_Address_file_mirror);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);

			fprintf(f_ptr, "Erase_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
		}

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);


	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);

	scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	fclose(f_ptr);
	return 0;
}


/*********************ADAPTED from PBTI_VG_ConstPulse************************/
/*****very similar to PBTI_VG_ConstPulse (VDD_DIG and {VDD_WL, VSS_WL} are stacked up to create higher than 1.7~1.8V WL output to VG)
except that VDS = VAB can be larger than 0 => a hybrid of PBIT and HCI/self-heating enhanced trapping*********/
int Stacked_VG_ConstPulse(char* Measure_file, double VDD_WL, double VSS_WL, double VDD_DIG, double* VDS, char* pulse_width_char, int chip, int col, int direction, int Num_of_Pulse, int Num_of_trigger){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, Num_of_trigger);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	/*	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0); */

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-ExtTrigger-ed measurements within a WLpulse
	//float NPLCycles = 1.0;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001]; //for multi-trigger/sample, innitiate->fetch data retrival

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0); //Need to update!!!
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, Num_of_trigger);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	for (row = 0; row < Num_of_row[col]; row++){
		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		/*******ATTENTION PULSE is still high!difference of NIDAQ signal (pulse vs MUXs' ENs) from 28nm!!!********/


		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		/*******BUG fixed: Pulse low now! (The difference of NIDAQ signal (pulse vs MUXs' ENs) from 28nm)********/

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		for (t = 1; t <= Num_of_Pulse; t++){

			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
			DO_USB6008(MUX_Address_file_stress);

			//scan("../Scan_files/MUX_ON", 0, 100000.0);

			//multiple triggers, single sample
			MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, Num_of_trigger);
			//char RdBuffer[12001];

			_ibwrt(_MM34401A, "INITiate");
			_ibwrt(_MM34410A_6, "INITiate");

			/********MODIFICATION**********/
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_DIG);
			E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, VSS_WL);
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDS[row]);     //VA=VDS
			//E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS[row]); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
			//E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS[row]); //VDD_WL=VGS
			/********MODIFICATION**********/

			//Only 40ms and 5000ms PULSE files have the built-in innitial delay -- 30ms NOpulse!!! (which brings down pulse)
			//PULSE waveforms!!!

			// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
			// before toggling PULSE and trigger Isub measurment
			//multiple triggers, single sample
			scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
			E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0);

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			_ibwrt(_MM34401A, "FETCh?");
			_ibrd(_MM34401A, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < Num_of_trigger; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_ID_program=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			_ibwrt(_MM34410A_6, "FETCh?");
			_ibrd(_MM34410A_6, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < Num_of_trigger; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_Isub=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
			/*E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);*/
			DO_USB6008(MUX_Address_file_stress);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);

			DO_USB6008(MUX_Address_file_mirror);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
			/*******BUG: Pulse is not low yet!!! The difference of NIDAQ signal (pulse vs MUXs' ENs) from 28nm!!!********/
			/********cells are inacturality stressed longer than I thought************/

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
		}

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	int recovery = 1; //roughly characterize short-term recovery every (10+)mins, all rows together

	for (int r = 0; r < recovery; r++){
		GetLocalTime(&lt);
		fprintf(f_ptr, "The local time is: %02d:%02d:%02d\nRetention: short-term recovery\n", lt.wHour, lt.wMinute, lt.wSecond);
		::Sleep(300000);
		::Sleep(300000);
		GetLocalTime(&lt);
		fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

		// scan in WL[0]=1 in column[col], pulse=0
		sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
		scan(f_scan, 0, 100000.0);

		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		for (row = 0; row < Num_of_row[col]; row++){
			//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
			fprintf(f_ptr, "WL[%d]\n", row);
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
			DO_USB6008(MUX_Address_file_stress);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
			//debug: TODO these printf's waste resource/time???
			//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

			DO_USB6008(MUX_Address_file_mirror);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
			//debug:
			//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

			// shift down one row
			scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
		}
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	}

	scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	fclose(f_ptr);
	return 0;
}

/*********************ADAPTED from stress_VG_ConstPulse************************/
/********BUG not fixed yet!!! PULSE still high even MUX_OFF!!!*****************/
/*****very similar to cell-wise stress_VG_ConstPulse, except that VDD_DIG and {VDD_WL, VSS_WL} are stacked up to create higher than 1.7~1.8V WL output to VG*********/
int PBTI_VG_ConstPulse(char* Measure_file, double VDD_WL, double VSS_WL, double VDD_DIG, char* pulse_width_char, int chip, int col, int direction, int Num_of_Pulse, int Num_of_trigger){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, Num_of_trigger);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	/*	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0); */

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-ExtTrigger-ed measurements within a WLpulse
	//float NPLCycles = 1.0;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001]; //for multi-trigger/sample, innitiate->fetch data retrival

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0); //Need to update!!!
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, Num_of_trigger);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	for (row = 0; row < Num_of_row[col]; row++){
		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		/*******ATTENTION PULSE is still high!difference of NIDAQ signal (pulse vs MUXs' ENs) from 28nm!!!********/


		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		/*******BUG: Pulse is not low yet!!! The difference of NIDAQ signal (pulse vs MUXs' ENs) from 28nm!!!********/
		/********cells are inacturality stressed longer than I thought************/   


		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		for (t = 1; t <= Num_of_Pulse; t++){

			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
			DO_USB6008(MUX_Address_file_stress);

			//scan("../Scan_files/MUX_ON", 0, 100000.0);

			//multiple triggers, single sample
			MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, Num_of_trigger);
			//char RdBuffer[12001];

			_ibwrt(_MM34401A, "INITiate");
			_ibwrt(_MM34410A_6, "INITiate");

			/********MODIFICATION**********/
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_DIG);
			E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, VSS_WL);
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL);
			//E3646A_SetVoltage(_VSPARE_VAB, 2, VDS[row]);     //VA=VDS
			//E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS[row]); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
			//E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS[row]); //VDD_WL=VGS
			/********MODIFICATION**********/

			//Only 40ms and 5000ms PULSE files have the built-in innitial delay -- 30ms NOpulse!!! (which brings down pulse)
			//PULSE waveforms!!!
		
			// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
			// before toggling PULSE and trigger Isub measurment
			//multiple triggers, single sample
			scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
			E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			//E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			_ibwrt(_MM34401A, "FETCh?");
			_ibrd(_MM34401A, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < Num_of_trigger; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_ID_program=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			_ibwrt(_MM34410A_6, "FETCh?");
			_ibrd(_MM34410A_6, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < Num_of_trigger; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_Isub=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
			/*E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);*/
			DO_USB6008(MUX_Address_file_stress);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);

			DO_USB6008(MUX_Address_file_mirror);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
			/*******BUG: Pulse is not low yet!!! The difference of NIDAQ signal (pulse vs MUXs' ENs) from 28nm!!!********/
			/********cells are inacturality stressed longer than I thought************/

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
		}

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\nRetention: short-term recovery\n", lt.wHour, lt.wMinute, lt.wSecond);
	::Sleep(300000);
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	fclose(f_ptr);
	return 0;
}

/*********************ADAPTED from stress_VG_ConstPulse************************/
int MLC_programming(char* Measure_file, double VDS, double VGS, char* pulse_width_char, int chip, int col, int direction, int Max_Num_of_Pulse, int Max_Num_of_Pulse_round2, double IDSAT_threshold){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, 1);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	//row: WL[row] number (row=0~Num_of_row[col]), j: number of ExtTrig measurement within one WLpulse here == 1.
	int row, j;
	float NPLCycles = 0.2;  //integration time for ExtTrigger-ed measurements within a WLpulse
	//float NPLCycles = 1.0;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001]; //for multi-trigger/sample, innitiate->fetch data retrival

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0); //Need to update!!!
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled


		fprintf(f_ptr, "ALL_Initial_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Initial_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	int pulse;                   //count the number of stress pulse cycles.
	int Next_pulse[128]; //an array keeping track of whether each row needs to be stressed in the next pulse cycle
	//C++ syntax doesn't allow the array length to be a variable Num_of_row[col], so I have to declare the longest column length
	int Next_pulse_round2[128]; 
	// 2 iteration rounds: give relaxed cell a second chance to amend stress pulse(s) to compensate for the recovery; but NO IMMEDIATE actions on them in the 1st round!
	// --> Next_pulse[0:127] can only be changed from "1" to "0"
	//Next_pulse_round2 keep real track of all_IDSAT during round=1, and can be changed from "0" to "1" if some cells relaxed too much back
	//it will amend stress pulse(s) to those cells in a 2nd round after all Next_pulse = 0 during the 1st round.
	//during the 2nd round, Next_pulse_round2[128] can only change from "1" to "0" (similar to Next_pulse[128] in round=1)
	
	for (row = 0; row < 128; row++){
		Next_pulse[row] = 0;  //first initialize the whole array to all 0's
		Next_pulse_round2[row] = 0;
	}
	for (row = 0; row < Num_of_row[col]; row++){
		Next_pulse[row] = 1;     // the real rows initialized to all 1's because all these rows need at least the first pulse
		Next_pulse_round2[row] = 1;
	}                                // this array will be overwritten/updated as stress pulse cycles proceed.
	int Rows_remain; //the number of rows still haven't reached the threshold
	int Rows_remain_round2;//during round=1: the REAL number of rows still haven't reached the threshold, including cells relaxed back!


	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, 1);

        int Max_Pulse;
	int round; //the 1st round: normal stress-check naive algorithm
	           //the 2nd round: ammend compensatory pulse(s) on relaxed cells

        for (round = 1; round <= 2; round++){
	    if (round == 1){
		    Max_Pulse = Max_Num_of_Pulse;
	    }
	    if (round == 2){
	            Max_Pulse = Max_Num_of_Pulse_round2;
	    }

	    for (pulse = 1; pulse <= Max_Pulse; pulse++){

	    	// scan in WL[0]=1 in column[col], pulse=0
	    	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	    	scan(f_scan, 0, 100000.0);

	    	SYSTEMTIME lt;
	    	GetLocalTime(&lt);
	    	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	    	for (row = 0; row < Num_of_row[col]; row++){
	    		
	    		fprintf(f_ptr, "WL[%d]\n", row);
	    		fprintf(f_ptr, "round=%d, Pulse_Cycle=%d, Next_Pulse=%d, Next_pulse_round2=%d\n", round, pulse, Next_pulse[row], Next_pulse_round2[row]);

	    		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);

	    		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
	    		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	    		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	    		DO_USB6008(MUX_Address_file_stress);
	    		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
	    		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

	    		/***************BUG fixed!**************/
	    		scan("../Scan_files/NOpulse", 0, 100000.0);
	    		/*******NOpulse after finishing measurement, before using any injection********/

	    		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	    		fprintf(f_ptr, "Before_round%d_%dPULSE_IDSAT_WL[%d]_%s=%.12f\n", round, pulse, row, direction_char_stress, Isense);
	    		//debug: TODO these printf's waste resource/time???
	    		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

	    		DO_USB6008(MUX_Address_file_mirror);
	    		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
	    		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

	    		/***************BUG fixed!**************/
	    		scan("../Scan_files/NOpulse", 0, 100000.0);
	    		/*******NOpulse after finishing measurement, before using any injection********/

	    		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	    		fprintf(f_ptr, "Before_round%d_%dPULSE_IDSAT_WL[%d]_%s=%.12f\n", round, pulse, row, direction_char_mirror, Isense);
	    		//debug:
	    		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

	    //		for (t = 1; t <= Num_of_Pulse; t++){

	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	    		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	    		DO_USB6008(MUX_Address_file_stress);

	    		//scan("../Scan_files/MUX_ON", 0, 100000.0);

	    		if(((round == 1) && (Next_pulse[row] == 1))||((round == 2) && (Next_pulse_round2[row] == 1))){
	    			//multiple triggers, single sample
	    		/*	MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, 1);
	    			//char RdBuffer[12001];

	    			_ibwrt(_MM34401A, "INITiate");
	    			_ibwrt(_MM34410A_6, "INITiate");
	    		*/
	    			E3646A_SetVoltage(_VSPARE_VAB, 2, VDS);     //VA=VDS
	    			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
	    			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS); //VDD_WL=VGS

	    			// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
	    			// before toggling PULSE and trigger Isub measurment
	    			//multiple triggers, single sample
	    			scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

	    			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
	    			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
	    			E3646A_SetVoltage(_VSPARE_VAB, 2, 0);

	    			/***************BUG fixed!**************/
	    			scan("../Scan_files/NOpulse", 0, 100000.0);
	    			/*******NOpulse after finishing measurement, before using any injection********/

	    			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	    	/*		_ibwrt(_MM34401A, "FETCh?");
	    			_ibrd(_MM34401A, RdBuffer, 12000);
	    			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
	    			//printf("%s\n", RdBuffer);

	    			for (j = 0; j < 1; j++){
	    				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
	    				//each reading has 15 charaters, plus one comma
	    				strncpy(StrCurrent, RdBuffer + j * 16, 15);
	    				StrCurrent[15] = '\0';
	    				sscanf(StrCurrent, "%f", &Current[j]);
	    				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_ID_program=%.12f\n", pulse, row, Current[j]);
	    				//debug:
	    				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
	    			}

	    			_ibwrt(_MM34410A_6, "FETCh?");
	    			_ibrd(_MM34410A_6, RdBuffer, 12000);
	    			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
	    			//printf("%s\n", RdBuffer);

	    			for (j = 0; j < 1; j++){
	    				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
	    				//each reading has 15 charaters, plus one comma
	    				strncpy(StrCurrent, RdBuffer + j * 16, 15);
	    				StrCurrent[15] = '\0';
	    				sscanf(StrCurrent, "%f", &Current[j]);
	    				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_Isub=%.12f\n", pulse, row, Current[j]);
	    				//debug:
	    				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
	    			}

	    			MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	    	*/
	    		}
	    		/*E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
	    		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	    		scan("../Scan_files/MUX_OFF", 0, 100000.0);*/
	    		DO_USB6008(MUX_Address_file_stress);
	    		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
	    		Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

	    		/***************BUG fixed!**************/
	    		scan("../Scan_files/NOpulse", 0, 100000.0);
	    		/*******NOpulse after finishing measurement, before using any injection********/

	    		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	    		fprintf(f_ptr, "Stress_round%d_%dPULSE_IDSAT_WL[%d]_%s=%.12f\n", round, pulse, row, direction_char_stress, Isense);
	    		//debug:
	    		//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);

	    		DO_USB6008(MUX_Address_file_mirror);
	    		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
	    		Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

	    		/***************BUG fixed!**************/
	    		scan("../Scan_files/NOpulse", 0, 100000.0);
	    		/*******NOpulse after finishing measurement, before using any injection********/

	    		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	    		fprintf(f_ptr, "Stress_round%d_%dPULSE_IDSAT_WL[%d]_%s=%.12f\n", round, pulse, row, direction_char_mirror, Isense);
	    		//debug:
	    		//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
	    		//}

	    		// shift down one row
	    		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	    	}
	    	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	    	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	    	GetLocalTime(&lt);
	    	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\nRetention: short-term recovery\n", lt.wHour, lt.wMinute, lt.wSecond);
	    	::Sleep(300000);
	    	GetLocalTime(&lt);
	    	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	    	// scan in WL[0]=1 in column[col], pulse=0
	    	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	    	scan(f_scan, 0, 100000.0);

	    	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	    	for (row = 0; row < Num_of_row[col]; row++){
	    		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	    		fprintf(f_ptr, "WL[%d]\n", row);
	    		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
	    		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	    		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	    		DO_USB6008(MUX_Address_file_stress);
	    		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
	    		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

	    		/***************BUG fixed!**************/
	    		scan("../Scan_files/NOpulse", 0, 100000.0);
	    		/*******NOpulse after finishing measurement, before using any injection********/

	    		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	    		fprintf(f_ptr, "ALL_Recovered_round%d_%dPULSE_IDSAT_WL[%d]_%s=%.12f\n", round, pulse, row, direction_char_stress, Isense);
	    		//debug: TODO these printf's waste resource/time???
	    		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

	    		DO_USB6008(MUX_Address_file_mirror);
	    		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
	    		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
	    		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

	    		/***************BUG fixed!**************/
	    		scan("../Scan_files/NOpulse", 0, 100000.0);
	    		/*******NOpulse after finishing measurement, before using any injection********/

	    		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	    		fprintf(f_ptr, "ALL_Recovered_round%d_%dPULSE_IDSAT_WL[%d]_%s=%.12f\n", round, pulse, row, direction_char_mirror, Isense);
	    		//debug:
	    		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

			if (round == 1){
			    if (Isense <= IDSAT_threshold){ //conpare reverse(mirror) IDSAT with the reference level threshold
	    		    	Next_pulse[row] = 0; //if reached the threshold, no longer apply pulse to this row!
	    		    	Next_pulse_round2[row] = 0;
	    		    }
	    		    if (Isense > IDSAT_threshold){
	    		    	Next_pulse_round2[row] = 1; //in the 1st round, Next_pulse_round2 can even change back to 1 if cell relaxed back!
	    		    }
			}

			if (round == 2){
			    if (Isense <= IDSAT_threshold){
				Next_pulse[row] = 0;
				Next_pulse_round2[row] = 0; // in the 2nd round, both Next_pulse and Next_pulse_round2 can only change from '1' to '0'.
			    }
			}

	    		// shift down one row
	    		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	    	} 
	    	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	    	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	    	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	    	Rows_remain = 0;
	    	Rows_remain_round2 = 0;
	    	for (row = 0; row < Num_of_row[col]; row++){
	    		Rows_remain += Next_pulse[row];
	    		Rows_remain_round2 += Next_pulse_round2[row];
	    	}

	    	fprintf(f_ptr, "\nround %d: after pulse cycle %d\nRows_remain=%d, Rows_remain_round2=%d\n\n", round, pulse, Rows_remain, Rows_remain_round2);

		if (round == 1){
		   if (Rows_remain == 0)
	    	   	break;
		}
		if (round == 2){
		   if (Rows_remain_round2 == 0)
			break;
		}
	    }
        }

	scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	//fprintf(f_ptr, "total pulse cycles: %d\n", pulse);
        //fprintf(f_ptr, "remaining number of rows didn't reach IDSAT=%f: %d\n", IDSAT_threshold, Rows_remain);

	fclose(f_ptr);
	return 0;
}


/*********************ADAPTED from stress_VG_ConstPulse ************************/
int us_stress_VG_ConstPulse(char* Measure_file, double* VDS, double* VGS, char* pulse_width_char, int is_us, int ExtTrig, int chip, int col, int direction, int Num_of_Pulse){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}

	char f_scan_WLpulse_ExtTrig[200];
	double samp_rate;
	if(is_us == 0){ 
	    samp_rate = 1000.0; // sampling rate = 1000 (1ms) is required to use these scan files with pulse >= 1ms!!!
	    /*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	    all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	    the "total number of triggers" = "WL PULSE width" / 10ms */
	    sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, 1);
	}

	if(is_us == 1){ //if pulse length is in micro-second range, using samp_rate=100000 (10us)
	    samp_rate = 100000.0; // sampling rate = 100000 (10us) when using scan files with pulse in us!!!
	    sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_100000SampRate", pulse_width_char, 1);
	}

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	/*	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0); */

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-ExtTrigger-ed measurements within a WLpulse
	//float NPLCycles = 1.0;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001]; //for multi-trigger/sample, innitiate->fetch data retrival

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0); //Need to update!!!
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled


		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	if(ExtTrig == 1){
	    MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, 1); 
	    //only exteral trigger measurement when pulse>1ms, so that NPLCycle=0.2 integration time can fit into one WL pulse
	    //scan files with pulse<=1ms do not have the external trigger (ignore what's said on their file names)
	}

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	for (row = 0; row < Num_of_row[col]; row++){
		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		for (t = 1; t <= Num_of_Pulse; t++){

			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
			DO_USB6008(MUX_Address_file_stress);

			//scan("../Scan_files/MUX_ON", 0, 100000.0);

			if(ExtTrig == 1){
			    //only exteral trigger measurement when pulse>1ms, so that NPLCycle=0.2 integration time can fit into one WL pulse
	    		    //scan files with pulse<=1ms is not using the external trigger for now (even NPLCycle=0.02 might not fit into the pulse)
			    //multiple triggers, single sample
			    MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, 1);
			    //char RdBuffer[12001];

			    _ibwrt(_MM34401A, "INITiate");
			    _ibwrt(_MM34410A_6, "INITiate");
			}

			E3646A_SetVoltage(_VSPARE_VAB, 2, VDS[row]);     //VA=VDS
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS[row]); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS[row]); //VDD_WL=VGS

			if(is_us == 0){
			    // 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
			    // before toggling PULSE and trigger Isub measurment
			    //multiple triggers, single sample
			    scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);
			}
			if(is_us == 1){
			    // 30ms initial delay is NOT built into the beginning of the scan file
			    ::Sleep(30); // 30ms delay before turning on VGS->WL short pulse to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle 
			    scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);
			}

			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0);

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			if(ExtTrig == 1){
			    //only exteral trigger measurement when pulse>1ms, so that NPLCycle=0.2 integration time can fit into one WL pulse
	    		    //scan files with pulse<=1ms is not using the external trigger for now (even NPLCycle=0.02 might not fit into the pulse)
			    _ibwrt(_MM34401A, "FETCh?");
			    _ibrd(_MM34401A, RdBuffer, 12000);
			    RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			    //printf("%s\n", RdBuffer);

			    for (j = 0; j < 1; j++){
			    	//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
			    	//each reading has 15 charaters, plus one comma
			    	strncpy(StrCurrent, RdBuffer + j * 16, 15);
			    	StrCurrent[15] = '\0';
			    	sscanf(StrCurrent, "%f", &Current[j]);
			    	fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_ID_program=%.12f\n", t, row, Current[j]);
			    	//debug:
			    	//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			    }

			    _ibwrt(_MM34410A_6, "FETCh?");
			    _ibrd(_MM34410A_6, RdBuffer, 12000);
			    RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			    //printf("%s\n", RdBuffer);

			    for (j = 0; j < 1; j++){
			    	//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
			    	//each reading has 15 charaters, plus one comma
			    	strncpy(StrCurrent, RdBuffer + j * 16, 15);
			    	StrCurrent[15] = '\0';
			    	sscanf(StrCurrent, "%f", &Current[j]);
			    	fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_Isub=%.12f\n", t, row, Current[j]);
			    	//debug:
			    	//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			    }
			}

			MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
			/*E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);*/
			DO_USB6008(MUX_Address_file_stress);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);

			DO_USB6008(MUX_Address_file_mirror);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
		}

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\nRetention: short-term recovery\n", lt.wHour, lt.wMinute, lt.wSecond);
	::Sleep(300000);
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	} 
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	fclose(f_ptr);
	return 0;
}

/*********************ADAPTED from Siming_28nm************************/
/*****************add SLEEP and SYSTEMTIME functions for more accurate control of short-term recovery*****************/
int stress_VG_ConstPulse(char* Measure_file, double* VDS, double* VGS, char* pulse_width_char, int chip, int col, int direction, int Num_of_Pulse){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, 1);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	/*	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0); */

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-ExtTrigger-ed measurements within a WLpulse
	//float NPLCycles = 1.0;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001]; //for multi-trigger/sample, innitiate->fetch data retrival

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0); //Need to update!!!
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled


		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, 1);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	for (row = 0; row < Num_of_row[col]; row++){
		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		for (t = 1; t <= Num_of_Pulse; t++){

			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
			DO_USB6008(MUX_Address_file_stress);

			//scan("../Scan_files/MUX_ON", 0, 100000.0);

			//multiple triggers, single sample
			MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, 1);
			//char RdBuffer[12001];

			_ibwrt(_MM34401A, "INITiate");
			_ibwrt(_MM34410A_6, "INITiate");

			E3646A_SetVoltage(_VSPARE_VAB, 2, VDS[row]);     //VA=VDS
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS[row]); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS[row]); //VDD_WL=VGS

			// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
			// before toggling PULSE and trigger Isub measurment
			//multiple triggers, single sample
			scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0);

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			_ibwrt(_MM34401A, "FETCh?");
			_ibrd(_MM34401A, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < 1; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_ID_program=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			_ibwrt(_MM34410A_6, "FETCh?");
			_ibrd(_MM34410A_6, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < 1; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_Isub=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
			/*E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);*/
			DO_USB6008(MUX_Address_file_stress);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);

			DO_USB6008(MUX_Address_file_mirror);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

			/***************BUG fixed!**************/
			scan("../Scan_files/NOpulse", 0, 100000.0);
			/*******NOpulse after finishing measurement, before using any injection********/

			//scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
		}

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\nRetention: short-term recovery\n", lt.wHour, lt.wMinute, lt.wSecond);
	::Sleep(300000);
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		fprintf(f_ptr, "ALL_Final_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	} 
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

	scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	fclose(f_ptr);
	return 0;
}


/*****************Use MM34410 to measure drain current, many sample ExtTriggers, track for a long time, to extract RTN *****************/
int RTN_ID_MM34410(char* Measure_file, double VDS, double VGS, char* scan_file_name, int chip, int col, int direction, int Num_of_ExtTrig, float NPLCycles){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // for now, sampling rate = 1000 for this scan files => each bit 1ms
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%s", scan_file_name);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	/*	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0); */

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, j;
	//float NPLCycles = 0.2;  //integration time for Multi-ExtTrigger-ed measurements within a WLpulse
	//float NPLCycles = 1.0;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[50001]; //DMM34410 can hold 50000 readings
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[800001]; //for multi-trigger/sample, innitiate->fetch data retrival

	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	fprintf(f_ptr, "The local time is: %02d:%02d:%02d\n", lt.wHour, lt.wMinute, lt.wSecond);

	for (row = 0; row < 1; row++){ //Test things out: only measure 1 row ;)
	//for (row = 0; row < Num_of_row[col]; row++){
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
		DO_USB6008(MUX_Address_file_stress);

		//scan("../Scan_files/MUX_ON", 0, 100000.0);

		_ibwrt(_MM34410A_6, "INITiate");

		E3646A_SetVoltage(_VSPARE_VAB, 2, VDS);     //VA=VDS
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS); //VDD_WL=VGS

		// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
		// before toggling PULSE and trigger ID measurment
		//multiple triggers, single sample
		scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);

		/***************BUG fixed!**************/
		scan("../Scan_files/NOpulse", 0, 100000.0);
		/*******NOpulse after finishing measurement, before using any injection********/

		//scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled

		_ibwrt(_MM34410A_6, "FETCh?");
		_ibrd(_MM34410A_6, RdBuffer, 800000);
		RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
		//printf("%s\n", RdBuffer);

		for (j = 0; j < Num_of_ExtTrig; j++){
			//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
			//each reading has 15 charaters, plus one comma
			strncpy(StrCurrent, RdBuffer + j * 16, 15);
			StrCurrent[15] = '\0';
			sscanf(StrCurrent, "%f", &Current[j]);
			fprintf(f_ptr, "Sample_time_%05dx10ms_WL[%d]_ID=%.12f\n", j, row, Current[j]);
			//debug:
			//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
		}

	        GetLocalTime(&lt);
	        fprintf(f_ptr, "The local time is: %02d:%02d:%02d\nRetention: short-term recovery\n", lt.wHour, lt.wMinute, lt.wSecond);
		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}

	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	fclose(f_ptr);
	return 0;
}


int stress_VG_RampPulse_Isub(char* Measure_file, double* VDS, double* VGS, char* pulse_width_char, int chip, int col, int direction, int Num_of_Pulse){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, 1);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	int scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-ExtTrigger-ed measurements within a WLpulse
	//float NPLCycles = 1.0;
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001]; //for multi-trigger/sample, innitiate->fetch data retrival

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	for (row = 0; row < Num_of_row[col]; row++){
		//MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "ALL_Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);

	// scan in WL[0]=1 in column[col], pulse=0
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, NPLCycles, "EXT", 0.0, 1, 1);

	for (row = 0; row < Num_of_row[col]; row++){
		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		//printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		for (t = 1; t <= Num_of_Pulse; t++){

			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008(MUX_Address_file_stress);

			scan("../Scan_files/MUX_ON", 0, 100000.0);

			//multiple triggers, single sample
			MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, 1);
			//char RdBuffer[12001];

			_ibwrt(_MM34401A, "INITiate");
			_ibwrt(_MM34410A_6, "INITiate");

			E3646A_SetVoltage(_VSPARE_VAB, 2, VDS[row]);     //VA=VDS
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS[t - 1]); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS[t - 1]); //VDD_WL=VGS

			// 30ms initial delay is built into the beginning of the scan file to allow VDS/VDD_DIG/VDD_WL PSUs to transition->settle
			// before toggling PULSE and trigger Isub measurment
			//multiple triggers, single sample
			scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
			scan("../Scan_files/MUX_OFF", 0, 100000.0);

			_ibwrt(_MM34401A, "FETCh?");
			_ibrd(_MM34401A, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < 1; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_ID_program=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			_ibwrt(_MM34410A_6, "FETCh?");
			_ibrd(_MM34410A_6, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < 1; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_Isub=%.12f\n", t, row, Current[j]);
				//debug:
				//printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
			/*	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008(MUX_Address_file_stress); */
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);

			DO_USB6008(MUX_Address_file_mirror);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
			//debug:
			//printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
		}

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);

	scan_equal = scan_selfcheck("../Scan_files/NIDAQ_test_data", 1);
	fprintf(f_ptr, "scan equal =%d\n", scan_equal);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	fclose(f_ptr);
	return 0;
}

int stress_Ext_Imeas_1by1(char* Measure_file, double VDS, double VGS, char* pulse_width_char, int Num_of_ExtTrig, int chip, int col, int direction, int Num_of_Pulse){

	char direction_char_stress[200], direction_char_mirror[200];
	char MUX_Address_file_stress[200], MUX_Address_file_mirror[200];

	if (direction == 0){
		sprintf(direction_char_stress, "VAsource_VBdrain");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
		sprintf(direction_char_mirror, "VAdrain_VBsource");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	else{
		sprintf(direction_char_stress, "VAdrain_VBsource");
		sprintf(MUX_Address_file_stress, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
		sprintf(direction_char_mirror, "VAsource_VBdrain");
		sprintf(MUX_Address_file_mirror, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}


	double samp_rate = 1000.0; // sampling rate = 1000 is required to use these scan files!!!
	/*	"char* pulse_width_char" is used for naming the "WL PULSE scan file"
	all these "WL PULSE + ExtTrigger files" need to use 1000.0 sampling rate, to guarantee 10ms between ExtTrig;
	the "total number of triggers" = "WL PULSE width" / 10ms */
	char f_scan_WLpulse_ExtTrig[200];
	sprintf(f_scan_WLpulse_ExtTrig, "../Scan_files/%sPULSE_MUX_ON_%dExtTrig_1000SampRate", pulse_width_char, Num_of_ExtTrig);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);

	FILE *f_ptr;
	if ((f_ptr = fopen(Measure_file, "a")) == NULL){
		printf("Cannot open%s.\n", Measure_file);
		return FAIL;
	}

	//row: WL[row] number (row=0~107), t: the number of WLpulse (t=1~Num_of_Pulse), j: number of ExtTrig measurement within one WLpulse
	int row, t, j;
	float NPLCycles = 0.2;  //integration time for Multi-EstTrigger-ed measurements within a WLpulse
	float Isense;
	//the DMM's internal memory can hold 512 readings at most => the maximum number of (ExtTrig) measurements before fetching to output buffer
	float Current[512];
	//the output buffer is formatted as "SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,SD.DDDDDDDDSEDD,....", so 15+1(comma) = 16 characters for each reading
	char StrCurrent[16];
	char RdBuffer[12001];

	for (row = 0; row < Num_of_row[col]; row++){
		MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
		fprintf(f_ptr, "WL[%d]\n", row);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);
		DO_USB6008(MUX_Address_file_stress);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);
		//debug: TODO these printf's waste resource/time???
		printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_stress, Isense);

		DO_USB6008(MUX_Address_file_mirror);
		scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure IDSAT + leakage current through Current Meter
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
		scan("../Scan_files/MUX_OFF", 0, 100000.0);

		fprintf(f_ptr, "Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);
		//debug:
		printf("Fresh_IDSAT_WL[%d]_%s=%.12f\n", row, direction_char_mirror, Isense);

		for (t = 1; t <= Num_of_Pulse; t++){

			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008(MUX_Address_file_stress);

			scan("../Scan_files/MUX_ON", 0, 100000.0);

			//multiple triggers, single sample
			MM34401A_MeasCurrent_Config(_MM34401A, NPLCycles, "EXT", 0.0, 1, Num_of_ExtTrig);
			//char RdBuffer[12001];

			_ibwrt(_MM34401A, "INITiate");

			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS); //VDD_WL=VGS
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDS);     //VA=VDS

			//multiple triggers, single sample
			scan(f_scan_WLpulse_ExtTrig, 0, samp_rate);

			E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			scan("../Scan_files/MUX_OFF", 0, 100000.0);

			_ibwrt(_MM34401A, "FETCh?");
			//_ibwrt(_MM34401A, "READ?");
			_ibrd(_MM34401A, RdBuffer, 12000);
			RdBuffer[ibcntl] = '\0';         // Null terminate the ASCII string    
			//printf("%s\n", RdBuffer);

			for (j = 0; j < Num_of_ExtTrig; j++){
				//output buffer of the DMM is formatted as CSV, of all the readings fetched from its internal memory 
				//each reading has 15 charaters, plus one comma
				strncpy(StrCurrent, RdBuffer + j * 16, 15);
				StrCurrent[15] = '\0';
				sscanf(StrCurrent, "%f", &Current[j]);
				fprintf(f_ptr, "Stress_%02dPULSE_WL[%d]_ID_%03dms=%.12f\n", t, row, 10 * j, Current[j]);
				//debug:
				printf("Time=%dms, Current=%.12f\n", 10 * j, Current[j]);
			}

			MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
			/*	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); // change VDD_WL => Vgs of WL selected transisor
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);
			DO_USB6008(MUX_Address_file_stress); */
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);
			//debug:
			printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_stress, Isense);

			DO_USB6008(MUX_Address_file_mirror);
			scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
			E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VAB = VDS = VDD_typicalV
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
			scan("../Scan_files/MUX_OFF", 0, 100000.0);

			fprintf(f_ptr, "Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
			//debug:
			printf("Stress_%02dPULSE_IDSAT_WL[%d]_%s=%.12f\n", t, row, direction_char_mirror, Isense);
		}

		// shift down one row
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);
	fclose(f_ptr);
	return 0;
}

//TODO: fix the mistakes about MUX enable and pulse scan files!!!
int stress_checkerboard(int even_odd, double VDS, double VGS, double pulse_width, int chip, int col, int direction){
	/* stress half of the transistors in a certain column
	direction = 0: VAsource, VBdrain (VB=VDS=VD > VA=VS=0)
	direction = 1: VAdrain, VBsource (VA=VDS=VD > VB=VS=0)
	*** Do full I-V curves characteriations in both directions EVERYTIME before using this function! ***
	even_odd = 0 select even number rows (0, 2, 4, ..., 106)
	even_odd = 1 select odd number rows (1, 3, 5, ..., 107)
	VDS = VA, VGS = VDD_WL = VDD_DIG,
	pulse_width tougle WL within VDS adjustment window by PSU, in unit of SECOND.
	scan file "Singal_pulse" has 1000 lines of pulse=1, with a final line to reset pulse=0
	baseline default sampling rate=1000, which generates 1 second of pulse high.
	set pulse_width=10 will change sampling rate to 100, which generates 10 seconds of pulse high.
	TODO: double check data type of sampling rate!
	*/
	scan("../Scan_files/NIDAQ_test_data", 1, 100000.0);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}

	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008(MUX_Address_file);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);
	//TODO: scan file error check!
	if (even_odd == 1){
		scan("../Scan_files/Scan_shift_1_NOpulse", 0, 100000.0);
	}
	double samp_rate = 1000.0; // sampling rate = 1000 generates 1 second pulse=1
	samp_rate = samp_rate / pulse_width;
	// eg. samp_rate = 1000/5 = 200, to generate 5 seconds of pulse
	// eg. samp_rate = 1000/0.1 = 10000, to generate 0.1 seconds of pulse 

	scan("../Scan_files/MUX_ON", 0, 100000.0);
	for (int i = 0; i < 54; i++){
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VGS); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VGS); //VDD_WL=VGS
		E3646A_SetVoltage(_VSPARE_VAB, 2, VDS);     //VA=VDS

		scan("../Scan_files/Singal_pulse_MUX_ON", 0, samp_rate);

		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV

		// shift down two rows
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);
	return 0;
}

int stress_sweep_VGS_VDS(double pulse_width, int chip, int col, int direction){
	/* stress the transistors in a certain column
	direction = 0: VAsource, VBdrain (VB=VDS=VD > VA=VS=0)
	direction = 1: VAdrain, VBsource (VA=VDS=VD > VB=VS=0)
	*** Do full I-V curves characteriations in both directions EVERYTIME before using this function! ***
	Monitor! VDS <= V(PSU+), VGS = VDD_WL = VDD_DIG,

	pulse_width tougle WL within VDS adjustment window by PSU, in unit of SECOND.
	scan file "Singal_pulse_MUX_ON" has 1000 lines of pulse=1, with a final line to reset pulse=0
	baseline default sampling rate=1000, which generates 1 second of pulse high.
	set pulse_width=10 will change sampling rate to 100, which generates 10 seconds of pulse high.
	TODO: double check data type of sampling rate!
	*/
	scan("../Scan_files/NIDAQ_test_data", 1, 100000.0);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);

	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}

	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008(MUX_Address_file);

	// scan in WL[0]=1 in column[col], pulse=0
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_NOpulse", col);
	scan(f_scan, 0, 100000.0);
	//TODO: scan file error check!

	double samp_rate = 1000.0; // sampling rate = 1000 generates 1 second pulse=1
	samp_rate = samp_rate / pulse_width;
	// eg. samp_rate = 1000/5 = 200, to generate 5 seconds of WL pulse
	// eg. samp_rate = 1000/0.1 = 10000, to generate 0.1 seconds of WL pulse 

	scan("../Scan_files/MUX_ON", 0, 100000.0);
	for (int i = 0; i < 12; i++){
		/*	WL[9n]: don't stress, to check if {VG=0, VD high} causes degradation
		WL[9n+1]: VDS=0, VGS=1.5
		WL[9n+2]: VDS=0, VGS=1.6
		WL[9n+3]: VDS=0, VGS=1.7
		WL[9n+4]: VDS=0, VGS=1.8
		WL[9n+5]: VDS=0, VGS=1.9
		WL[9n+6]: VDS=0, VGS=2.0
		WL[9n+7]: VDS=0, VGS=2.1
		WL[9n+8]: VDS=0, VGS=2.2
		*/
		//WL[9n]
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);  // shift down 1 row
		//WL[9n+1]
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, 1.5); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, 1.5); //VDD_WL=VGS
		//		E3646A_SetVoltage(_VSPARE_VAB, 2, 1.8);     //VA=VDS
		scan("../Scan_files/Singal_pulse_MUX_ON", 0, samp_rate);
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);  // shift down 1 row
		//WL[9n+2]
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, 1.6); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, 1.6); //VDD_WL=VGS
		//		E3646A_SetVoltage(_VSPARE_VAB, 2, 1.8);     //VA=VDS
		scan("../Scan_files/Singal_pulse_MUX_ON", 0, samp_rate);
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);  // shift down 1 row
		//WL[9n+3]
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, 1.7); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, 1.7); //VDD_WL=VGS
		//		E3646A_SetVoltage(_VSPARE_VAB, 2, 2.0);     //VA=VDS
		scan("../Scan_files/Singal_pulse_MUX_ON", 0, samp_rate);
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);  // shift down 1 row
		//WL[9n+4]
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, 1.8); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, 1.8); //VDD_WL=VGS
		//		E3646A_SetVoltage(_VSPARE_VAB, 2, 2.0);     //VA=VDS
		scan("../Scan_files/Singal_pulse_MUX_ON", 0, samp_rate);
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);  // shift down 1 row
		//WL[9n+5]
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, 1.9); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, 1.9); //VDD_WL=VGS
		//		E3646A_SetVoltage(_VSPARE_VAB, 2, 2.0);     //VA=VDS
		scan("../Scan_files/Singal_pulse_MUX_ON", 0, samp_rate);
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);  // shift down 1 row
		//WL[9n+6]
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, 2.0); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, 2.0); //VDD_WL=VGS
		//		E3646A_SetVoltage(_VSPARE_VAB, 2, 2.2);     //VA=VDS
		scan("../Scan_files/Singal_pulse_MUX_ON", 0, samp_rate);
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);  // shift down 1 row
		//WL[9n+7]
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, 2.1); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, 2.1); //VDD_WL=VGS
		//		E3646A_SetVoltage(_VSPARE_VAB, 2, 2.2);     //VA=VDS
		scan("../Scan_files/Singal_pulse_MUX_ON", 0, samp_rate);
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV
		scan("../Scan_files/Scan_shift_1_NOpulse_MUX_ON", 0, 100000.0);  // shift down 1 row
		//WL[9n+8]
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, 2.2); //VDD_DIG=VDD_WL=VGS, avoid any un-intentional crowbar current or turn-on diodes
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, 2.2); //VDD_WL=VGS
		//		E3646A_SetVoltage(_VSPARE_VAB, 2, 2.2);     //VA=VDS
		scan("../Scan_files/Singal_pulse_MUX_ON", 0, samp_rate);
		E3646A_SetVoltage(_VSPARE_VAB, 2, 0);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); //VDD_WL=VDD_typicalV
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); //VDD_DIG=VDD_typicalV

	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);
	return 0;
}

/******** ADAPTED from IDS_VGS, only added one line: to change VDS together with VGS: VDS=VGS=VDD_WL **********/
/******** IDSAT of diode connected nmos (VDS=VGS=VDD_WL) during supply voltage scaling **********/
int IDS_diode(char *f_name, int col, int chip, int direction)
//direction = 0: VAsource, VBdrain
//direction = 1: VAdrain, VBsource
{
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	// 1) scan in all Zero's, measure total leakage current through a single column VA --> VB
	// sweep VDD_WL, total leakage should not make a difference!
	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	double Isense;
	FILE *f_ptr;
	if ((f_ptr = fopen(f_name, "a")) == NULL){
		printf("Cannot open%s.\n", f_name);
		return FAIL;
	}

	double leakage, VDD_WL;
	fprintf(f_ptr, "Total leakage current from column[%d], chip %d, VAB=VDS=%fV\n%s\n", col, chip, VDD_typical, direction_char);
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	DO_USB6008(MUX_Address_file); //MUX enable embedded into mux address file, all using USB-NIDAQ
	//scan("../Scan_files/MUX_ON", 0, 100000.0);

	E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VDS = |VB - VA|= VDD_typicalV
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	leakage = MM34401A_MeasCurrent(_MM34401A);
	if (leakage >= 0.00001){
		//		printf("total subVt leakage too large! Check!\n");
		fprintf(f_ptr, "total subVt leakage too large! Check!\n");
		//		fclose(f_ptr);
		//		return FAIL;
	}
	//debug:
	//	printf("Leakage = %.12f\n", leakage);
	for (VDD_WL = VDD_typical; VDD_WL >= 0.2 - 0.0001; VDD_WL -= 0.05){
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL); // change VDD_WL
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure leakage current through Current Meter
		fprintf(f_ptr, "VDD_WL=%f  Isense=%.12f\n", VDD_WL, Isense);
		if (fabs(Isense - leakage) >= 0.000001){
			//			printf("total subVt leakage should not change with VDD_WL! Check!\n");
			fprintf(f_ptr, "total subVt leakage should not change with VDD_WL! Check!\n");
			//			fclose(f_ptr);
			//			return FAIL;
		}
		//debug:
		//		printf("VDD_WL=%f  Isense=%.12f\n", VDD_WL, Isense);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);

	fprintf(f_ptr, "\nIds-Vgs curves of each fresh transistor, from 0 to %d, in column[%d], chip %d\n%s\n", Num_of_row[col] - 1, col, chip, direction_char);
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	//MM34401A_MeasCurrent_Config(1);
	// scan in WL[0]=1 in column[col], pulse=1
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_pulse", col);
	scan(f_scan, 0, 100000.0);

	//scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	DO_USB6008(MUX_Address_file); //MUX enable embedded into mux address file, all using USB-NIDAQ
	E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VDS = |VB - VA|= VDD_typicalV
	int WL;
	for (WL = 0; WL < Num_of_row[col]; WL++){
		fprintf(f_ptr, "WL[%d]\n", WL);
		for (VDD_WL = VDD_typical; VDD_WL >= 0.2 - 0.0001; VDD_WL -= 0.05){
			//only added the following line compared to IDS_VGS function
	                E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_WL); // VDS = |VB - VA|= VDD_WL
			//only added the above line compared to IDS_VGS function
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL); // change VDD_WL => Vgs of WL selected transisor
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			fprintf(f_ptr, "VDD_WL=%f  Isense=%.12f  I(sense)-I(leakage)=%.12f\n", VDD_WL, Isense, Isense - leakage);
			//CAUTIOUS: the leakage current measurment and calculations are wrong here (remnant from IDS_VGS function)
			//if necessary, need to use the method (I_leak for a certain VDS) in IDS_VDS function. 
		}
		//shift scan bit to the next WL
		scan("../Scan_files/Scan_shift_1", 0, 100000.0);
	}
	scan("../Scan_files/NOpulse", 0, 100000.0);
	fclose(f_ptr);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	return 0;
}

/******************ADAPTED from Siming_28nm******************/
int IDS_VGS(char *f_name, int col, int chip, int direction)
//direction = 0: VAsource, VBdrain
//direction = 1: VAdrain, VBsource
{
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	// 1) scan in all Zero's, measure total leakage current through a single column VA --> VB
	// sweep VDD_WL, total leakage should not make a difference!
	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	double Isense;
	FILE *f_ptr;
	if ((f_ptr = fopen(f_name, "a")) == NULL){
		printf("Cannot open%s.\n", f_name);
		return FAIL;
	}

	double leakage, VDD_WL;
	fprintf(f_ptr, "Total leakage current from column[%d], chip %d, VAB=VDS=%fV\n%s\n", col, chip, VDD_typical, direction_char);
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	DO_USB6008(MUX_Address_file); //MUX enable embedded into mux address file, all using USB-NIDAQ
	//scan("../Scan_files/MUX_ON", 0, 100000.0);

	E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VDS = |VB - VA|= VDD_typicalV
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	leakage = MM34401A_MeasCurrent(_MM34401A);
	if (leakage >= 0.00001){
		//		printf("total subVt leakage too large! Check!\n");
		fprintf(f_ptr, "total subVt leakage too large! Check!\n");
		//		fclose(f_ptr);
		//		return FAIL;
	}
	//debug:
	//	printf("Leakage = %.12f\n", leakage);
	for (VDD_WL = VDD_typical; VDD_WL >= 0.2 - 0.0001; VDD_WL -= 0.05){
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL); // change VDD_WL
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure leakage current through Current Meter
		fprintf(f_ptr, "VDD_WL=%f  Isense=%.12f\n", VDD_WL, Isense);
		if (fabs(Isense - leakage) >= 0.000001){
			//			printf("total subVt leakage should not change with VDD_WL! Check!\n");
			fprintf(f_ptr, "total subVt leakage should not change with VDD_WL! Check!\n");
			//			fclose(f_ptr);
			//			return FAIL;
		}
		//debug:
		//		printf("VDD_WL=%f  Isense=%.12f\n", VDD_WL, Isense);
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);

	fprintf(f_ptr, "\nIds-Vgs curves of each fresh transistor, from 0 to %d, in column[%d], chip %d\n%s\n", Num_of_row[col] - 1, col, chip, direction_char);
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	//MM34401A_MeasCurrent_Config(1);
	// scan in WL[0]=1 in column[col], pulse=1
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_pulse", col);
	scan(f_scan, 0, 100000.0);

	//scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	DO_USB6008(MUX_Address_file); //MUX enable embedded into mux address file, all using USB-NIDAQ
	E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VDS = |VB - VA|= VDD_typicalV
	int WL;
	for (WL = 0; WL < Num_of_row[col]; WL++){
		fprintf(f_ptr, "WL[%d]\n", WL);
		for (VDD_WL = VDD_typical; VDD_WL >= 0.2 - 0.0001; VDD_WL -= 0.05){
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL); // change VDD_WL => Vgs of WL selected transisor
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			fprintf(f_ptr, "VDD_WL=%f  Isense=%.12f  I(sense)-I(leakage)=%.12f\n", VDD_WL, Isense, Isense - leakage);
			//debug:
			//			printf("VDD_WL=%f  Isense=%.12f  I(sense)-I(leakage)=%.12f\n", VDD_WL, Isense, Isense - leakage);
		}
		//shift scan bit to the next WL
		scan("../Scan_files/Scan_shift_1", 0, 100000.0);
	}
	scan("../Scan_files/NOpulse", 0, 100000.0);
	fclose(f_ptr);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	return 0;
}

/******************ADAPTED from IDS_VGS******************/
/******************Adding more voltage for charaterization******************/
int IDS_sweepVG(char *f_name, int col, int chip, int direction, float VD, float VS, float VB, float VGmin, float VGmax)
//direction = 0: VAsource, VBdrain
//direction = 1: VAdrain, VBsource
{
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	// 1) scan in all Zero's, measure total leakage current through a single column VA --> VB
	// sweep VDD_WL, total leakage should not make a difference!
	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	double Isense;
	FILE *f_ptr;
	if ((f_ptr = fopen(f_name, "a")) == NULL){
		printf("Cannot open%s.\n", f_name);
		return FAIL;
	}

	double leakage, VDD_WL;
	//Modified:
	fprintf(f_ptr, "Total leakage current from column[%d], chip %d\n%s\n", col, chip, direction_char);
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	DO_USB6008(MUX_Address_file); //MUX enable embedded into mux address file, all using USB-NIDAQ
	//scan("../Scan_files/MUX_ON", 0, 100000.0);

	//Modified:
	E3646A_SetVoltage(_VSPARE_VAB, 1, VS); // VS = VSPARE
	E3646A_SetVoltage(_VSPARE_VAB, 2, VD); // VD = VAB
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VB); // VB = VSS_PW
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	leakage = MM34401A_MeasCurrent(_MM34401A);
	if (leakage >= 0.00001){
		//		printf("total subVt leakage too large! Check!\n");
		fprintf(f_ptr, "total subVt leakage too large! Check!\n");
		//		fclose(f_ptr);
		//		return FAIL;
	}
	//debug:
	//	printf("Leakage = %.12f\n", leakage);
	//Modified:
	for (VDD_WL = VGmax; VDD_WL >= VGmin - 0.0001; VDD_WL -= 0.05){
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL); // change VDD_WL
		if (VDD_WL > VDD_typical){// change VDD_DIG=VDD_WL, if VDD_WL domain needs to be higher than scan chain domain
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_WL); //otherwise driving from lower to higher supply is problematic
		}//(driving from higher voltage into lower voltage domain should be fine, and need to ensure scan FF retention
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure leakage current through Current Meter
		fprintf(f_ptr, "VDD_WL=%f  Isense=%.12f\n", VDD_WL, Isense);
		if (fabs(Isense - leakage) >= 0.000001){
			//			printf("total subVt leakage should not change with VDD_WL! Check!\n");
			fprintf(f_ptr, "total subVt leakage should not change with VDD_WL! Check!\n");
			//			fclose(f_ptr);
			//			return FAIL;
		}
		//debug:
		//		printf("VDD_WL=%f  Isense=%.12f\n", VDD_WL, Isense);
	}
	//Modified:
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0); // VB = VSS_PW
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); // VS = VSPARE
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical);

	fprintf(f_ptr, "\nIds-Vgs curves of each transistor, from 0 to %d, in column[%d], chip %d\n%s\n", Num_of_row[col] - 1, col, chip, direction_char);
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	//MM34401A_MeasCurrent_Config(1);
	// scan in WL[0]=1 in column[col], pulse=1
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_pulse", col);
	scan(f_scan, 0, 100000.0);

	//scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	DO_USB6008(MUX_Address_file); //MUX enable embedded into mux address file, all using USB-NIDAQ
	//Modified:
	E3646A_SetVoltage(_VSPARE_VAB, 1, VS); // VS = VSPARE
	E3646A_SetVoltage(_VSPARE_VAB, 2, VD); // VD = VAB
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VB); // VB = VSS_PW

	int WL;
	for (WL = 0; WL < Num_of_row[col]; WL++){
		fprintf(f_ptr, "WL[%d]\n", WL);
		//Modified:
		for (VDD_WL = VGmax; VDD_WL >= VGmin - 0.0001; VDD_WL -= 0.05){
			E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL); // change VDD_WL => Vgs of WL selected transisor
			if (VDD_WL > VDD_typical){// change VDD_DIG=VDD_WL, if VDD_WL domain needs to be higher than scan chain domain
				E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_WL); //otherwise driving from lower to higher supply is problematic
			}//(driving from higher voltage into lower voltage domain should be fine, and need to ensure scan FF retention
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			//Modified:
			fprintf(f_ptr, "VDD_WL=%f  Isense=%.12f\n", VDD_WL, Isense);
			//debug:
			//			printf("VDD_WL=%f  Isense=%.12f  I(sense)-I(leakage)=%.12f\n", VDD_WL, Isense, Isense - leakage);
		}
		//shift scan bit to the next WL
		scan("../Scan_files/Scan_shift_1", 0, 100000.0);
	}
	scan("../Scan_files/NOpulse", 0, 100000.0);
	fclose(f_ptr);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical);
	//Modified:
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0); // VB = VSS_PW
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); // VS = VSPARE
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	return 0;
}


/******************ADAPTED from IDS_sweepVG******************/
int IDS_sweepVD(char *f_name, int col, int chip, int direction, float VG, float VS, float VB, float VDmin, float VDmax)
//direction = 0: VAsource, VBdrain
//direction = 1: VAdrain, VBsource
{
	//	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	// 1) scan in all Zero's, measure total leakage current through a single column VA --> VB
	// sweep VD
	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	double Isense;
	FILE *f_ptr;
	if ((f_ptr = fopen(f_name, "a")) == NULL){
		printf("Cannot open%s.\n", f_name);
		return FAIL;
	}

	//Modified:
	double leakage, VD_sweep;
	//Modified:
	fprintf(f_ptr, "Total leakage current from column[%d], chip %d\n%s\n", col, chip, direction_char);
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	DO_USB6008(MUX_Address_file); //MUX enable embedded into mux address file, all using USB-NIDAQ
	//scan("../Scan_files/MUX_ON", 0, 100000.0);

	//Modified:
	E3646A_SetVoltage(_VSPARE_VAB, 1, VS); // VS = VSPARE
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VG); // change VDD_WL => VG of WL selected transisor
	if (VG > VDD_typical){// change VDD_DIG=VDD_WL, if VDD_WL domain needs to be higher than scan chain domain
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VG); //otherwise driving from lower to higher supply is problematic
	}//(driving from higher voltage into lower voltage domain should be fine, and need to ensure scan FF retention
	E3646A_SetVoltage(_VSPARE_VAB, 2, VDmax); // VD = VDmax
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VB); // VB = VSS_PW
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	leakage = MM34401A_MeasCurrent(_MM34401A);
	if (leakage >= 0.00001){
		//		printf("total subVt leakage too large! Check!\n");
		fprintf(f_ptr, "total subVt leakage too large! Check!\n");
		//		fclose(f_ptr);
		//		return FAIL;
	}
	//debug:
	//	printf("Leakage = %.12f\n", leakage);
	//Modified:
	for (VD_sweep = VDmax; VD_sweep >= VDmin - 0.0001; VD_sweep -= 0.05){
		//Modified:
	        E3646A_SetVoltage(_VSPARE_VAB, 2, VD_sweep); // change VD = VD_sweep
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure leakage current through Current Meter
		fprintf(f_ptr, "VD=%f  Isense=%.12f\n", VD_sweep, Isense);
		if (fabs(Isense - leakage) >= 0.000001){
			//			printf("total subVt leakage should not change with VDD_WL! Check!\n");
			fprintf(f_ptr, "total subVt leakage should not change too much! Check!\n");
			//			fclose(f_ptr);
			//			return FAIL;
		}
		//debug:
		//		printf("VDD_WL=%f  Isense=%.12f\n", VDD_WL, Isense);
	}
	//Modified:
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0); // VB = VSS_PW
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); // VS = VSPARE
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical);

	fprintf(f_ptr, "\nIds-Vds curves of each transistor, from 0 to %d, in column[%d], chip %d\n%s\n", Num_of_row[col] - 1, col, chip, direction_char);
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	//MM34401A_MeasCurrent_Config(1);
	// scan in WL[0]=1 in column[col], pulse=1
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_pulse", col);
	scan(f_scan, 0, 100000.0);

	//scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	DO_USB6008(MUX_Address_file); //MUX enable embedded into mux address file, all using USB-NIDAQ
	//Modified:
	E3646A_SetVoltage(_VSPARE_VAB, 1, VS); // VS = VSPARE
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VG); // change VDD_WL => VG of WL selected transisor
	if (VG > VDD_typical){// change VDD_DIG=VDD_WL, if VDD_WL domain needs to be higher than scan chain domain
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VG); //otherwise driving from lower to higher supply is problematic
	}//(driving from higher voltage into lower voltage domain should be fine, and need to ensure scan FF retention
	E3646A_SetVoltage(_VSPARE_VAB, 2, VDmax); // VD = VDmax
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VB); // VB = VSS_PW

	int WL;
	for (WL = 0; WL < Num_of_row[col]; WL++){
		fprintf(f_ptr, "WL[%d]\n", WL);
		//Modified:
		for (VD_sweep = VDmax; VD_sweep >= VDmin - 0.0001; VD_sweep -= 0.05){
			E3646A_SetVoltage(_VSPARE_VAB, 2, VD_sweep); // change VD = VD_sweep
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leakage current through Current Meter
			fprintf(f_ptr, "VD=%f  Isense=%.12f\n", VD_sweep, Isense);
			//debug:
			//			printf("VDD_WL=%f  Isense=%.12f  I(sense)-I(leakage)=%.12f\n", VDD_WL, Isense, Isense - leakage);
		}
		//shift scan bit to the next WL
		scan("../Scan_files/Scan_shift_1", 0, 100000.0);
	}
	scan("../Scan_files/NOpulse", 0, 100000.0);
	fclose(f_ptr);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical);
	//Modified:
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0); // VB = VSS_PW
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); // VS = VSPARE
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled
	return 0;
}
//Cautious: out-of-date!!
int IDS_VDS(char* f_name, int col, int chip, int direction){

	// 1) scan in all Zero's, measure total leakage current through a single column VA --> VB
	// reduce VAB=VDS, total leakage should decrease
	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}

	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008(MUX_Address_file);
	//DEBUG:
	/*	float I_V_NIDAQ = 0.001*E3646A_MeasAvgCurrent(_VDD_IO_V_NIDAQ, 2);
	float I_VAB = 0.001*E3646A_MeasAvgCurrent(_VSPARE_VAB, 2);
	printf("Before MUX_ON: I(V_NIDAQ) = %f, I(VAB) = %f\n", I_V_NIDAQ, I_VAB);*/

	scan("../Scan_files/MUX_ON", 0, 100000.0);
	//DEBUG:
	/*	I_V_NIDAQ = 0.001*E3646A_MeasAvgCurrent(_VDD_IO_V_NIDAQ, 2);
	I_VAB = 0.001*E3646A_MeasAvgCurrent(_VSPARE_VAB, 2);
	printf("After MUX_ON: I(V_NIDAQ) = %f, I(VAB) = %f\n", I_V_NIDAQ, I_VAB);*/

	//	E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); // VDS = |VB - VA|= VDD_typicalV
	double Isense;
	FILE *f_ptr;
	if ((f_ptr = fopen(f_name, "a")) == NULL){
		printf("Cannot open%s.\n", f_name);
		return FAIL;
	}
	//Vgs=0, total leakage vs Vds
	double leak_tot[19];
	fprintf(f_ptr, "Total leakage current from column[%d], chip %d, Vgs=0, sweep VAB=Vds\n%s\n", col, chip, direction_char);
	fprintf(f_ptr, "Total Leakage through column[%d]:\n", col);
	float VAB;
	int l = 18;
	//	MM34401A_MeasCurrent_Config(100);
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	for (VAB = VDD_typical; VAB >= -0.00001; VAB -= 0.05){
		E3646A_SetVoltage(_VSPARE_VAB, 2, VAB);
		Isense = MM34401A_MeasCurrent(_MM34401A); //measure leakage current through Current Meter
		fprintf(f_ptr, "VAB=%f  Isense=%.12f\n", VAB, Isense);
		leak_tot[l] = Isense;
		l--;
		//debug:
		printf("VAB=%f  Isense=%.12f\n", VAB, Isense);
		/*		I_V_NIDAQ = 0.001*E3646A_MeasAvgCurrent(_VDD_IO_V_NIDAQ, 2);
		I_VAB = 0.001*E3646A_MeasAvgCurrent(_VSPARE_VAB, 2);
		printf("I(V_NIDAQ) = %f, I(VAB) = %f\n", I_V_NIDAQ, I_VAB);*/
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);

	fprintf(f_ptr, "\nIds-Vds curves of each fresh transistor, from 0 to 107, in column[%d]\n%s\n", col, direction_char);
	// scan in WL[0]=1 in column[col], pulse=1
	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_WL0_pulse", col);
	scan(f_scan, 0, 100000.0);
	int WL;
	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_ON_pulse", 0, 100000.0);
	for (WL = 0; WL < Num_of_row[col]; WL++){
		fprintf(f_ptr, "WL[%d]\n", WL);
		l = 0;
		for (VAB = 0; VAB <= VDD_typical + 0.001; VAB += 0.05){
			E3646A_SetVoltage(_VSPARE_VAB, 2, VAB); // change VAB => Vds of WL selected transistor
			Isense = MM34401A_MeasCurrent(_MM34401A); //measure Ids + leak_tot current through Current Meter
			fprintf(f_ptr, "VAB=%f  Isense=%.12f  I(sense)-I(leak_tot)=%.12f\n", VAB, Isense, Isense - leak_tot[l]);
			//debug:
			printf("VAB=%f  Vsense=%.12f  I(sense)-I(leak_tot)=%.12f\n", VAB, Isense, Isense - leak_tot[l]);
			l++;
		}
		scan("../Scan_files/Scan_shift_1_MUX_ON", 0, 100000.0);
	}
	scan("../Scan_files/NOpulse", 0, 100000.0);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	scan("../Scan_files/MUX_OFF", 0, 100000.0);
	fclose(f_ptr);
	return 0;
}

/******************ADAPTED from Drain_leakage******************/
/******************Add scan-in allWL of a column for column-wise punchthrough in "weak-inversion"****************/
int Col_punchthrough(char* f_name, double VS, double VB, double VG, int col, int chip, int direction, int MUX_ON){

//	double VD_max = 1.3;
	double VD_max = 2.4;
//	double VD_max = 1.4;
	//double VD_max = 2.8;
	// 1) scan in all Zero's, measure total leakage current through a single column VA --> VB
	// reduce VAB=VDS, total leakage should decrease
	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}

	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	if (MUX_ON == 1){
		DO_USB6008(MUX_Address_file);
		//scan("../Scan_files/MUX_ON", 0, 100000.0);
	}
	// otherwise (MUX_ON == 0): keep mux disconnected, 
	// to measure current components (Is, Id, Isub) at various (VS, VD, VB) voltages
	// without any column being connected to PSU+ and PSU-
	// used to cancel out these currents when plotting data
	// also to moniter whether these current components change overtime due to degradation

	double Isense_Isub, Isense_ID, Isense_IS;
	FILE *f_ptr;
	if ((f_ptr = fopen(f_name, "a")) == NULL){
		printf("Cannot open%s.\n", f_name);
		return FAIL;
	}

	if (MUX_ON == 1){
		fprintf(f_ptr, "Total leakage current from column[%d], chip %d, Vg=%f, Vs=%f, Vb=%f, sweep VAB=Vd\n%s\n", col, chip, VG, VS, VB, direction_char);
		fprintf(f_ptr, "Total Leakage through column[%d]:\n", col);
	}
	if (MUX_ON == 0){
		fprintf(f_ptr, "Total leakage current when MUX_OFF, chip %d, Vg=%f, Vs=%f, Vb=%f, sweep VAB=Vd\n%s\n", chip, VG, VS, VB, direction_char);
	}
	float VAB;

	char f_scan[200];
	sprintf(f_scan, "../Scan_files/Scan_Col%02d_allWL_Pulse", col);
	scan(f_scan, 0, 100000.0);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VG); //VDD_WL = VG
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VG); //VDD_DIG = VG
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
	E3646A_SetVoltage(_VSPARE_VAB, 1, VS); //VSPARE=VS
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VB);    //VSS_PW=VB

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, 10, "IMM", 0.1, 1, 1);
	MM34401A_MeasCurrent_Config(_MM34401A_7, 10, "IMM", 0.1, 1, 1);

	for (VAB = 0; VAB <= VD_max + 0.00001; VAB += 0.1){
		E3646A_SetVoltage(_VSPARE_VAB, 2, VAB);
		Isense_Isub = MM34401A_MeasCurrent(_MM34410A_6); //measure substrate current through Current Meter
		Isense_ID = MM34401A_MeasCurrent(_MM34401A); //measure Drain side leakage current through Current Meter
		Isense_IS = MM34401A_MeasCurrent(_MM34401A_7); //measure Source side leakage current through Current Meter
		fprintf(f_ptr, "VAB=%f  Isub=%.12f\n", VAB, Isense_Isub);
		fprintf(f_ptr, "VAB=%f  ID=%.12f\n", VAB, Isense_ID);
		fprintf(f_ptr, "VAB=%f  IS=%.12f\n", VAB, Isense_IS);
//		fprintf(f_ptr, "VAB=%f  I_VDD_WL=%.12f\n", VAB, Isense_IS);
		if (VAB > VD_max - 0.1 / 2.0){
			::Sleep(60000); //hold 1min at VD_max = 2.4V
			//Temperory coding, need more elegant program later!
		}
		
	}
	scan("../Scan_files/NOpulse", 0, 100000.0);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); //VSPARE=VS
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);    //VSS_PW=VB
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical); 
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical); 
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	fclose(f_ptr);
	return 0;
}


/******************ADAPTED from Siming_28nm******************/
/******************Add DMM for Is measurement****************/
int Drain_leakage(char* f_name, double VS, double VB, double VG, int col, int chip, int direction, int MUX_ON){

	double VD_max = 0.0;
//	double VD_max = 2.4;
//	double VD_max = 1.4;
//	double VD_max = 2.8;
	// 1) scan in all Zero's, measure total leakage current through a single column VA --> VB
	// reduce VAB=VDS, total leakage should decrease
	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}

//	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	if (MUX_ON == 1){
		DO_USB6008(MUX_Address_file);
		//scan("../Scan_files/MUX_ON", 0, 100000.0);
	}
	// otherwise (MUX_ON == 0): keep mux disconnected, 
	// to measure current components (Is, Id, Isub) at various (VS, VD, VB) voltages
	// without any column being connected to PSU+ and PSU-
	// used to cancel out these currents when plotting data
	// also to moniter whether these current components change overtime due to degradation

	double Isense_Isub, Isense_ID, Isense_IS;
	FILE *f_ptr;
	if ((f_ptr = fopen(f_name, "a")) == NULL){
		printf("Cannot open%s.\n", f_name);
		return FAIL;
	}

	if (MUX_ON == 1){
		fprintf(f_ptr, "Total leakage current from column[%d], chip %d, Vg=%f, Vs=%f, Vb=%f, sweep VAB=Vd\n%s\n", col, chip, VG, VS, VB, direction_char);
		fprintf(f_ptr, "Total Leakage through column[%d]:\n", col);
	}
	if (MUX_ON == 0){
		fprintf(f_ptr, "Total leakage current when MUX_OFF, chip %d, Vg=%f, Vs=%f, Vb=%f, sweep VAB=Vd\n%s\n", chip, VG, VS, VB, direction_char);
	}
	float VAB;

	E3646A_SetVoltage(_VSPARE_VAB, 1, VS); //VSPARE=VS
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VB);    //VSS_PW=VB

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, 10, "IMM", 0.1, 1, 1);
	MM34401A_MeasCurrent_Config(_MM34401A_7, 10, "IMM", 0.1, 1, 1);

	for (VAB = 0; VAB <= VD_max + 0.00001; VAB += 0.1){
		E3646A_SetVoltage(_VSPARE_VAB, 2, VAB);
		Isense_Isub = MM34401A_MeasCurrent(_MM34410A_6); //measure substrate current through Current Meter
		Isense_ID = MM34401A_MeasCurrent(_MM34401A); //measure Drain side leakage current through Current Meter
		Isense_IS = MM34401A_MeasCurrent(_MM34401A_7); //measure Source side leakage current through Current Meter
		fprintf(f_ptr, "VAB=%f  Isub=%.12f\n", VAB, Isense_Isub);
		fprintf(f_ptr, "VAB=%f  ID=%.12f\n", VAB, Isense_ID);
		fprintf(f_ptr, "VAB=%f  IS=%.12f\n", VAB, Isense_IS);
//		fprintf(f_ptr, "VAB=%f  I_VDD_WL=%.12f\n", VAB, Isense_IS);
		if (VAB > VD_max - 0.1 / 2.0){
			//::Sleep(60000); //hold 1min at VD_max = 2.4V
			::Sleep(600000); //hold 10min at VD_max = 2.4V
			//::Sleep(6000000); //hold 10min at VD_max = 2.4V
			//::Sleep(21600000); //hold 6 hours
			//Temperory coding, need more elegant program later!
		}
		
	}
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); //VSPARE=VS
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);    //VSS_PW=VB
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	fclose(f_ptr);
	return 0;
}

/******************ADAPTED from Drain_leakage******************/
/******************Add DMM for Is measurement****************/
int FN_Erase_leakage(char* f_name, double VSVBVD_max, double VG, double VDD_WL, int col, int chip, int direction, int MUX_ON){

//	double VD_max = 2.8;
	
	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}

	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); // VS = VSPARE = 0V
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VD = VAB = 0V
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	if (MUX_ON == 1){
		DO_USB6008(MUX_Address_file);
		//scan("../Scan_files/MUX_ON", 0, 100000.0);
	}
	// otherwise (MUX_ON == 0): keep mux disconnected, 
	// to measure current components (Is, Id, Isub) at various (VS, VD, VB) voltages
	// without any column being connected to PSU+ and PSU-
	// used to cancel out these currents when plotting data
	// also to moniter whether these current components change overtime due to degradation

	double Isense_Isub, Isense_ID, Isense_IS;
	FILE *f_ptr;
	if ((f_ptr = fopen(f_name, "a")) == NULL){
		printf("Cannot open%s.\n", f_name);
		return FAIL;
	}

	if (MUX_ON == 1){
		fprintf(f_ptr, "Total leakage current from column[%d], chip %d, Vg=%f, sweep VSVBVD from 0 to %f\nVSS_WL=0, VDD_WL=VDD_DIG=%f\n", col, chip, VG, VSVBVD_max, VDD_WL);
		fprintf(f_ptr, "Total Leakage through column[%d]:\n", col);
	}
	if (MUX_ON == 0){
		fprintf(f_ptr, "Total leakage current when MUX_OFF, chip %d, Vg=%f, sweep VSVBVD from 0 to %f\nVSS_WL=0, VDD_WL=VDD_DIG=%f\n", chip, VG, VSVBVD_max, VDD_WL);
	}

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, 10, "IMM", 0.1, 1, 1);
	MM34401A_MeasCurrent_Config(_MM34401A_7, 10, "IMM", 0.1, 1, 1);

	E3646A_SetVoltage(_VSPARE_VAB, 1, 0);       //VSPARE=VS
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0);       //VAB=VD
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);    //VSS_PW=VB

	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_WL);  //VDD_DIG=VDD_WL
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL);

	for (double VSVBVD = 0; VSVBVD <= VSVBVD_max + 0.00001; VSVBVD += 0.1){
		E3646A_SetVoltage(_VSPARE_VAB, 1, VSVBVD);       //VSPARE=VS
		E3646A_SetVoltage(_VSPARE_VAB, 2, VSVBVD);       //VAB=VD
		E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, VSVBVD);    //VSS_PW=VB
		
		::Sleep(1000); //to make sure voltages all settle down, therefore currents reach steady state 
		//(all "floating nodes" settle down to balance state)

		Isense_Isub = MM34401A_MeasCurrent(_MM34410A_6); //measure substrate current through Current Meter
		Isense_ID = MM34401A_MeasCurrent(_MM34401A); //measure Drain side leakage current through Current Meter
		Isense_IS = MM34401A_MeasCurrent(_MM34401A_7); //measure Source side leakage current through Current Meter
		fprintf(f_ptr, "VSVBVD=%f  Isub=%.12f\n", VSVBVD, Isense_Isub);
		fprintf(f_ptr, "VSVBVD=%f  ID=%.12f\n", VSVBVD, Isense_ID);
		fprintf(f_ptr, "VSVBVD=%f  IS=%.12f\n", VSVBVD, Isense_IS);
	}

	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);    //VSS_PW=VB 
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VAB = VD
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); // VSPARE=VS
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical);
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	fclose(f_ptr);
	return 0;
}

/******************ADAPTED from Drain_leakage******************/
/******************compare with typical IDS-VGS curves****************/
int VG_realvalue(char* f_name, double VDD_WL_max, int col, int chip, int direction){

	char direction_char[200];
	char MUX_Address_file[200];
	if (direction == 0){
		sprintf(direction_char, "VA = source, VB = drain");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAsource_VBdrain", col);
	}
	else{
		sprintf(direction_char, "VA = drain, VB = source");
		sprintf(MUX_Address_file, "../Scan_files/MUX_Col%02d_VAdrain_VBsource", col);
	}

	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 1, 0);
	scan("../Scan_files/Scan_all_zero", 0, 100000.0);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	DO_USB6008(MUX_Address_file);
	//scan("../Scan_files/MUX_ON", 0, 100000.0);

	double Isense_Isub, Isense_ID, Isense_IS;
	FILE *f_ptr;
	if ((f_ptr = fopen(f_name, "a")) == NULL){
		printf("Cannot open%s.\n", f_name);
		return FAIL;
	}

	fprintf(f_ptr, "Total current from column[%d], chip %d, scan in all zero, VS=VB=0, VAB=VD=%f, sweep VDD_DIG=VDD_WL from %f to %f\n%s\n", col, chip, VDD_typical, VDD_typical, VDD_WL_max, direction_char);

	MM34401A_MeasCurrent_Config(_MM34401A, 10, "IMM", 0.1, 1, 1);
	MM34410A_6_MeasCurrent_Config(_MM34410A_6, 10, "IMM", 0.1, 1, 1);
	MM34401A_MeasCurrent_Config(_MM34401A_7, 10, "IMM", 0.1, 1, 1);

	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); //VSPARE=VS
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);    //VSS_PW=VB
	E3646A_SetVoltage(_VSPARE_VAB, 2, VDD_typical); //VAB=VD=0.8

	for (double VDD_WL = VDD_typical; VDD_WL<= VDD_WL_max + 0.00001; VDD_WL += 0.1){
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_WL);
		E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_WL);
		Isense_Isub = MM34401A_MeasCurrent(_MM34410A_6); //measure substrate current through Current Meter
		Isense_ID = MM34401A_MeasCurrent(_MM34401A); //measure Drain side leakage current through Current Meter
		Isense_IS = MM34401A_MeasCurrent(_MM34401A_7); //measure Source side leakage current through Current Meter
		fprintf(f_ptr, "VDD_WL=%f  Isub=%.12f\n", VDD_WL, Isense_Isub);
		fprintf(f_ptr, "VDD_WL=%f  ID=%.12f\n", VDD_WL, Isense_ID);
		fprintf(f_ptr, "VDD_WL=%f  IS=%.12f\n", VDD_WL, Isense_IS);
	}
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 2, VDD_typical);
	E3646A_SetVoltage(_VDD_DIG_VDD_WL, 1, VDD_typical);
	E3646A_SetVoltage(_VSPARE_VAB, 2, 0); // VDS = |VB - VA|= 0V
	E3646A_SetVoltage(_VSPARE_VAB, 1, 0); //VSPARE=VS
	E3646A_SetVoltage(_VSS_WL_VSS_PW, 2, 0);    //VSS_PW=VB
	DO_USB6008("../Scan_files/MUX_OFF"); //all mux disabled, POLARITY[1:2]=0
	//scan("../Scan_files/MUX_OFF", 0, 100000.0);
	fclose(f_ptr);
	return 0;
}

/******************************
Scan Chain Load Function
******************************/

int scan(char *fn_scanin, int compareMode, float samp_freq)
{
	// NIDAQ-6115 (PCI card) 
	// digital output port for timing critical signals
	// "dev1/port0/line0:7": 
	// line0,   line1,   line2,   line3,   line4,                                  line5,        line6,       line7
	// S_IN,    PHI_1,   PHI_2,  PULSE_IN, unused (reserved for DMM ex-trigger),   /CS(unused), /WR(unused) /EN(unused).
	// analog input port "Dev1/ai0" monitor scan out from chip: S_OUT (VDD_IO=1.8V)

#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

	int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void *callbackData);
	//This function is defined in the counter output example code

	// Variables for Error Buffer
	int32 error = 0;
	char errBuff[2048] = { '\0' };

	//NIDAQ ANSI C example code, counter/digital pulse train continuous generation
	//we use this counter output for the external clock source of digital output and analog input channel sampling clock
	TaskHandle  taskHandleCounter0 = 0;
	DAQmxErrChk(DAQmxCreateTask("", &taskHandleCounter0));
	//The output frequency is the target frequency for the sampling clock.
	DAQmxErrChk(DAQmxCreateCOPulseChanFreq(taskHandleCounter0, "Dev1/ctr0", "", DAQmx_Val_Hz, DAQmx_Val_Low, 0.0, samp_freq, 0.50));
	//continuous output, specify a large enough buffer size
	DAQmxErrChk(DAQmxCfgImplicitTiming(taskHandleCounter0, DAQmx_Val_ContSamps, 1000));
	DAQmxErrChk(DAQmxRegisterDoneEvent(taskHandleCounter0, 0, DoneCallback, NULL));

	/*********************************************/
	// DAQmx Start Code
	/*********************************************/
	//We SHOULD NOT start the counter task now!!
	//	DAQmxErrChk(DAQmxStartTask(taskHandleCounter0));
	//	printf("Generating pulse train. Press Enter to interrupt\n");
	//	getchar();

	//Variables for Processing of Input lines (output to chip)
	TaskHandle taskHandleScanin = 0;
	TaskHandle taskHandleScanout = 0;
	uInt8 scaninData[MAX_LINES][NUM_OUTPUT];

	// bit-wise digital line format:
	//	uInt8 scaninData[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	int scanoutData[MAX_LINES];

	int total_lines_read = 0;
	int SCAN_EQUAL = 1;

	float64 anaInputBuffer[MAX_LINES];
	int32 read, bytesPerSamp;

	int linecount = 0;

	/*********************************************/
	//  FILE I/O Intialization
	/*********************************************/

	//file IO variables
	FILE *fptr_scanin;

	if ((fptr_scanin = fopen(fn_scanin, "r")) == NULL){
		printf("Cannot open%s.\n", fn_scanin);
		return FAIL;
	}

	//printf("DEBUG:scanin file found\n");

	/*********************************************/
	// READ vectors from input file
	/*********************************************/
	int input_char;

	scanChainRead_Error = 0;

	//loop for each line of file I/O
	while (((input_char = fgetc(fptr_scanin)) != EOF) && (linecount <  MAX_LINES)) {
		//loop to read an input line
		int inputcnt = 0;
		while (input_char != '\n'){
			if (input_char == '1'){
				//for bit-wise digital format:
				//				scaninData[linecount] = (scaninData[linecount]<<1) + 1;
				scaninData[linecount][inputcnt] = 1;
				inputcnt++;
			}
			else if ((input_char == '0') | (input_char == 'x') |
				(input_char == 'X') | (input_char == 'z') |
				(input_char == 'Z')){
				//				scaninData[linecount] = (scaninData[linecount]<<1) + 0;
				scaninData[linecount][inputcnt] = 0;
				inputcnt++;
			}

			if ((input_char = fgetc(fptr_scanin)) == EOF)
				break;
		}

		if (inputcnt != NUM_OUTPUT){
			printf("Input line %d was not formatted correctly, received %d input characters\n", linecount, inputcnt);
			printf("Data: ");
			for (int i = 0; i<NUM_OUTPUT; i++)
				//				printf ("%d ", scaninData[linecount][i]);
				printf("\n");
			scanChainRead_Error = 1;
		}
		linecount++;
		//printf("DEBUG: Read line %d\n", linecount);
	}
	fclose(fptr_scanin);
	total_lines_read = linecount;
	//printf("DEBUG: Finished Reading input file\n");

	/*********************************************/
	// DAQmx Configure Code
	/*********************************************/
	DAQmxErrChk(DAQmxCreateTask("OutputTask", &taskHandleScanin));
	DAQmxErrChk(DAQmxCreateDOChan(taskHandleScanin, outchannel_str, "outchannel", DAQmx_Val_ChanForAllLines));
	//implicit timing means software control, i.e. NIDAQ generate one sample output once it receives a software program command.
	//the timing is undeterministic because the variation in software runtime speed.
	//	DAQmxCfgImplicitTiming (taskHandleScanin, DAQmx_Val_FiniteSamps,1);

	//Configure an output buffer equal or larger than the number of output lines
	DAQmxErrChk(DAQmxCfgOutputBuffer(taskHandleScanin, total_lines_read));

	// For NI6115 device, "OnBoardClock", (MasterTimebase) and "" or NULL are not legal samplig clock source
	// While "20MHzTimebase" and "100kHzTimebase" are fixed frequency clock source that are not impacted by the sampling rate settings
	//	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandleScanin, "MasterTimebase", 1000, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, 14));

	//Using the counter ouput, set the sampling rate as the same as the counter task setting
	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandleScanin, "/dev1/ctr0Out", samp_freq, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, total_lines_read));

	DAQmxErrChk(DAQmxCreateTask("ReadTask", &taskHandleScanout));
	//TODO: adjust AI range to increase resolution!
	DAQmxErrChk(DAQmxCreateAIVoltageChan(taskHandleScanout, inchannel_str, "inchannel", DAQmx_Val_Cfg_Default, 0.0, VDD_NIDAQ + 0.1, DAQmx_Val_Volts, NULL));
	//Also use the counter ouput to synchronize with digital output generation, set the sampling rate as the same as the counter task setting
	//	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandleScanin, "/dev1/ctr0out", 1000.0, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, 108 * 12));
	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandleScanout, "/dev1/ctr0out", samp_freq, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, total_lines_read));
	//the AI task should have an implicitly allocated buffer to hold all acquired data after StartTask before ReadAnalogF64.

	/*********************************************/
	// DAQmx Start Code
	/*********************************************/
	// Do NOT start task here!
	// For buffered/hard-timed generation task, we need to first write all the data into buffer before starting the task
	//	DAQmxErrChk (DAQmxStartTask(taskHandleScanin));
	//	DAQmxErrChk (DAQmxStartTask(taskHandleScanout));

	/*********************************************/
	// DAQmx Read and Write Code - Loop
	/*********************************************/
	// if compareMode == 1, scan in twice, and compare the second-time scan out data with scan in data
	for (int scan_times = 0; scan_times <= compareMode; scan_times++){

		for (linecount = 0; linecount < total_lines_read; linecount++){
			//Write Code
			//Write digital data into output buffer, who holds the data before StartTask for real generating output
			// For this DO task, explicitly calling CfgOutputBuffer is necessary: 
			// otherwise the implicitly allocated buffer size defaults to 1,
			// because WriteDigitalLines is called one line at a time in a for loop!
			DAQmxErrChk(DAQmxWriteDigitalLines(taskHandleScanin, 1, 0, 20.0, DAQmx_Val_GroupByChannel, scaninData[linecount], NULL, NULL));
			// for bit-wise digital format
			//			DAQmxErrChk(DAQmxWriteDigitalU8(taskHandleScanin, 14, 0, 10.0, DAQmx_Val_GroupByChannel, scaninData, NULL, NULL));
			//			DAQmxErrChk(DAQmxStartTask(taskHandleScanin));
		}


		//first start the digital output and analog input tasks, 
		//but because the counter task has not started yet, no sampling clock ticks, these two tasks are ready and waiting
		DAQmxErrChk(DAQmxStartTask(taskHandleScanin));
		if (scan_times == 1){
			DAQmxErrChk(DAQmxStartTask(taskHandleScanout));
		}
		DAQmxErrChk(DAQmxStartTask(taskHandleCounter0));
		//		DAQmxErrChk(DAQmxReadAnalogF64(taskHandleScanout, -1, 10.0, DAQmx_Val_GroupByChannel, anaInputBuffer, total_lines_read, &read, NULL));
		//Now start the counter task, to guarantee input and output tasks start together and synchronized
		// may not start together if starting the counter task first!
		// Synchronised to counter0 sampling clock, DO generate samples from the previously filled output buffer, AI acquires samples into default internal buffer
		if (scan_times == 1){
			// DAQmxReadAnalogF64 read the acquired AI data from internal default buffer into the specified array "anaInputBuffer"
			DAQmxErrChk(DAQmxReadAnalogF64(taskHandleScanout, -1, 20.0, DAQmx_Val_GroupByChannel, anaInputBuffer, total_lines_read, &read, NULL));
			DAQmxStopTask(taskHandleScanout);
		}
		// CHECK: ANSI C code example (finite_AI_ExtClk)
		// The correct order: 
		//(1) StartTask: Scanin, Scanout 
		//(2) StartTask: Counter0
		//(3) ReadAnalogF64: Scanout
		// After (1) and (2), program proceeds without a waiting even if the generation and acquisation are still in progress.
		// Program wait at (3) for it to finish for a maximum duration of timeout

		// wait indefinitely until the task is done
		int taskDone;
		DAQmxErrChk(taskDone = DAQmxWaitUntilTaskDone(taskHandleScanin, -1));

		DAQmxStopTask(taskHandleScanin);
		//		DAQmxStopTask(taskHandleScanout);
		DAQmxStopTask(taskHandleCounter0);
	}

	if (compareMode == 1){
		for (linecount = 0; linecount < total_lines_read; linecount++){
			// write scan out into scanoutData
			// S_OUT(VDD_IO) terminal from chip go through level shifter, shifted to VDD_NIDAQ = 5V
			if (anaInputBuffer[linecount] > VDD_NIDAQ * 0.5) {
				scanoutData[linecount] = 1;
			}
			else {
				scanoutData[linecount] = 0;
			}
		}
	}
	// Check if scan out is correct
	// use PHI_1 (scaninData[linecount][1]) to sample scanoutData

	// TODO: figure out why scan out samples are offset from scan in by about 21 sampling clock points!
	// DEFAULT DELAY cycles before the first AI sampling clock triggering? 
	// Understand how hard-wired AI (external/ NIDAQ board internal counter generated) sampling clock timing works!
	// route out and probe ai/SamplingClk from PFI7 (configure it as an output!)
	// Also refresh / double-check: chip internal circuit architecture -- scan chain arrangement / length??

	if (compareMode == 1) {
		for (linecount = 0; linecount < total_lines_read; linecount++){
			if (scaninData[linecount][1] == 1) {
				if (scanoutData[linecount] != scaninData[linecount][0]){
					SCAN_EQUAL = 0;
					printf("line=%d\n", linecount);
					printf("scanin=%d\n", scaninData[linecount][0]);
					printf("scanout=%d, AI=%f\n", scanoutData[linecount], anaInputBuffer[linecount]);
				}
			}
		}
	}

	/*******************************************/
	// Output
	/*******************************************/
	//printf("SCAN OKAY\n");
	if (compareMode == 1) {
		printf("SCAN_EQUAL: %d  \n", SCAN_EQUAL);
	}

Error:
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
	if (taskHandleScanin != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandleScanin);
		DAQmxClearTask(taskHandleScanin);
	}
	// Andrew added following section for taskHandleScanout
	if (taskHandleScanout != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandleScanout);
		DAQmxClearTask(taskHandleScanout);
	}
	// I added this for counter out
	if (taskHandleCounter0 != 0){
		DAQmxStopTask(taskHandleCounter0);
		DAQmxClearTask(taskHandleCounter0);
	}
	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);
	fclose(fptr_scanin);
	return 0;
}

int long_scan(char *fn_scanin, float samp_freq)
{
	// NIDAQ-6115 (PCI card) 
	// digital output port for timing critical signals
	// "dev1/port0/line0:7": 
	// line0,   line1,   line2,   line3,   line4,                                  line5,        line6,       line7
	// S_IN,    PHI_1,   PHI_2,  PULSE_IN, unused (reserved for DMM ex-trigger),   /CS(unused), /WR(unused) /EN(unused).
	// analog input port "Dev1/ai0" monitor scan out from chip: S_OUT (VDD_IO=1.8V)

#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

	int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void *callbackData);
	//This function is defined in the counter output example code

	// Variables for Error Buffer
	int32 error = 0;
	char errBuff[2048] = { '\0' };

	//NIDAQ ANSI C example code, counter/digital pulse train continuous generation
	//we use this counter output for the external clock source of digital output and analog input channel sampling clock
	TaskHandle  taskHandleCounter0 = 0;
	DAQmxErrChk(DAQmxCreateTask("", &taskHandleCounter0));
	//The output frequency is the target frequency for the sampling clock.
	DAQmxErrChk(DAQmxCreateCOPulseChanFreq(taskHandleCounter0, "Dev1/ctr0", "", DAQmx_Val_Hz, DAQmx_Val_Low, 0.0, samp_freq, 0.50));
	//continuous output, specify a large enough buffer size
	DAQmxErrChk(DAQmxCfgImplicitTiming(taskHandleCounter0, DAQmx_Val_ContSamps, 1000));
	DAQmxErrChk(DAQmxRegisterDoneEvent(taskHandleCounter0, 0, DoneCallback, NULL));

	/*********************************************/
	// DAQmx Start Code
	/*********************************************/
	//We SHOULD NOT start the counter task now!!
	//	DAQmxErrChk(DAQmxStartTask(taskHandleCounter0));
	//	printf("Generating pulse train. Press Enter to interrupt\n");
	//	getchar();

	//Variables for Processing of Input lines (output to chip)
	TaskHandle taskHandleScanin = 0;
//	TaskHandle taskHandleScanout = 0;
	uInt8 scaninData[NUM_OUTPUT];

	// bit-wise digital line format:
	//	uInt8 scaninData[14] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	int total_lines_read = 0;
	
	int32 read, bytesPerSamp;

	int linecount = 0;

	/*********************************************/
	//  FILE I/O Intialization
	/*********************************************/

	//file IO variables
	FILE *fptr_scanin;

	if ((fptr_scanin = fopen(fn_scanin, "r")) == NULL){
		printf("Cannot open%s.\n", fn_scanin);
		return FAIL;
	}

	//printf("DEBUG:scanin file found\n");

	/*********************************************/
	// READ vectors from input file
	/*********************************************/
	int input_char;

	scanChainRead_Error = 0;

	//loop for each line of file I/O
//	while (((input_char = fgetc(fptr_scanin)) != EOF) && (linecount <  MAX_LINES)) {
	while ((input_char = fgetc(fptr_scanin)) != EOF) {
		//loop to read an input line
		int inputcnt = 0;
		while (input_char != '\n'){
			if (input_char == '1'){
			//	scaninData[linecount][inputcnt] = 1;
				inputcnt++;
			}
			else if ((input_char == '0') | (input_char == 'x') |
				(input_char == 'X') | (input_char == 'z') |
				(input_char == 'Z')){
			//	scaninData[linecount][inputcnt] = 0;
				inputcnt++;
			}

			if ((input_char = fgetc(fptr_scanin)) == EOF)
				break;
		}

		if (inputcnt != NUM_OUTPUT){
			printf("Input line %d was not formatted correctly, received %d input characters\n", linecount, inputcnt);
			printf("Data: ");
			for (int i = 0; i<NUM_OUTPUT; i++)
				//				printf ("%d ", scaninData[linecount][i]);
				printf("\n");
			scanChainRead_Error = 1;
		}
		linecount++;
		//printf("DEBUG: Read line %d\n", linecount);
	}
	fclose(fptr_scanin);
	total_lines_read = linecount;
	//printf("DEBUG: Finished Reading input file\n");

	/*********************************************/
	// DAQmx Configure Code
	/*********************************************/
	DAQmxErrChk(DAQmxCreateTask("OutputTask", &taskHandleScanin));
	DAQmxErrChk(DAQmxCreateDOChan(taskHandleScanin, outchannel_str, "outchannel", DAQmx_Val_ChanForAllLines));
	//implicit timing means software control, i.e. NIDAQ generate one sample output once it receives a software program command.
	//the timing is undeterministic because the variation in software runtime speed.
	//	DAQmxCfgImplicitTiming (taskHandleScanin, DAQmx_Val_FiniteSamps,1);

	//Configure an output buffer equal or larger than the number of output lines
	DAQmxErrChk(DAQmxCfgOutputBuffer(taskHandleScanin, total_lines_read));

	// For NI6115 device, "OnBoardClock", (MasterTimebase) and "" or NULL are not legal samplig clock source
	// While "20MHzTimebase" and "100kHzTimebase" are fixed frequency clock source that are not impacted by the sampling rate settings
	//	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandleScanin, "MasterTimebase", 1000, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, 14));

	//Using the counter ouput, set the sampling rate as the same as the counter task setting
	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandleScanin, "/dev1/ctr0Out", samp_freq, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, total_lines_read));

//	DAQmxErrChk(DAQmxCreateTask("ReadTask", &taskHandleScanout));
	//TODO: adjust AI range to increase resolution!
//	DAQmxErrChk(DAQmxCreateAIVoltageChan(taskHandleScanout, inchannel_str, "inchannel", DAQmx_Val_Cfg_Default, 0.0, VDD_NIDAQ + 0.1, DAQmx_Val_Volts, NULL));
	//Also use the counter ouput to synchronize with digital output generation, set the sampling rate as the same as the counter task setting
	//	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandleScanin, "/dev1/ctr0out", 1000.0, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, 108 * 12));
//	DAQmxErrChk(DAQmxCfgSampClkTiming(taskHandleScanout, "/dev1/ctr0out", samp_freq, DAQmx_Val_Rising, DAQmx_Val_FiniteSamps, total_lines_read));
	//the AI task should have an implicitly allocated buffer to hold all acquired data after StartTask before ReadAnalogF64.

	/*********************************************/
	// DAQmx Start Code
	/*********************************************/
	// Do NOT start task here!
	// For buffered/hard-timed generation task, we need to first write all the data into buffer before starting the task
	//	DAQmxErrChk (DAQmxStartTask(taskHandleScanin));
	//	DAQmxErrChk (DAQmxStartTask(taskHandleScanout));

	/*********************************************/
	// DAQmx Read and Write Code - Loop
	/*********************************************/
	// if compareMode == 1, scan in twice, and compare the second-time scan out data with scan in data

	if ((fptr_scanin = fopen(fn_scanin, "r")) == NULL){
		printf("Cannot open%s.\n", fn_scanin);
		return FAIL;
	}

	while ((input_char = fgetc(fptr_scanin)) != EOF) {
		//input_char = fgetc(fptr_scanin);
		//loop to read an input line
		int inputcnt = 0;
		while (input_char != '\n'){
			if (input_char == '1'){
				scaninData[inputcnt] = 1;
				inputcnt++;
			}
			else if ((input_char == '0') | (input_char == 'x') |
				(input_char == 'X') | (input_char == 'z') |
				(input_char == 'Z')){
				scaninData[inputcnt] = 0;
				inputcnt++;
			}

			if ((input_char = fgetc(fptr_scanin)) == EOF)
				break;
		}

		if (inputcnt != NUM_OUTPUT){
			printf("Input line %d was not formatted correctly, received %d input characters\n", linecount, inputcnt);
			printf("Data: ");
			for (int i = 0; i < NUM_OUTPUT; i++)
				//				printf ("%d ", scaninData[linecount][i]);
				printf("\n");
			scanChainRead_Error = 1;
		}
//		linecount++;
		//printf("DEBUG: Read line %d\n", linecount);
		
		//Write Code
		//Write digital data into output buffer, who holds the data before StartTask for real generating output
		// For this DO task, explicitly calling CfgOutputBuffer is necessary: 
		// otherwise the implicitly allocated buffer size defaults to 1,
		// because WriteDigitalLines is called one line at a time in a for loop!
		DAQmxErrChk(DAQmxWriteDigitalLines(taskHandleScanin, 1, 0, 20.0, DAQmx_Val_GroupByChannel, scaninData, NULL, NULL));
		// for bit-wise digital format
		//			DAQmxErrChk(DAQmxWriteDigitalU8(taskHandleScanin, 14, 0, 10.0, DAQmx_Val_GroupByChannel, scaninData, NULL, NULL));
		//			DAQmxErrChk(DAQmxStartTask(taskHandleScanin));
	}
	fclose(fptr_scanin);

	//first start the digital output and analog input tasks, 
	//but because the counter task has not started yet, no sampling clock ticks, these two tasks are ready and waiting
	DAQmxErrChk(DAQmxStartTask(taskHandleScanin));
/*	if (scan_times == 1){
		DAQmxErrChk(DAQmxStartTask(taskHandleScanout));
	}*/
	DAQmxErrChk(DAQmxStartTask(taskHandleCounter0));
	//		DAQmxErrChk(DAQmxReadAnalogF64(taskHandleScanout, -1, 10.0, DAQmx_Val_GroupByChannel, anaInputBuffer, total_lines_read, &read, NULL));
	//Now start the counter task, to guarantee input and output tasks start together and synchronized
	// may not start together if starting the counter task first!
	// Synchronised to counter0 sampling clock, DO generate samples from the previously filled output buffer, AI acquires samples into default internal buffer
/*	if (scan_times == 1){
		// DAQmxReadAnalogF64 read the acquired AI data from internal default buffer into the specified array "anaInputBuffer"
		DAQmxErrChk(DAQmxReadAnalogF64(taskHandleScanout, -1, 20.0, DAQmx_Val_GroupByChannel, anaInputBuffer, total_lines_read, &read, NULL));
		DAQmxStopTask(taskHandleScanout);
	}*/
	// CHECK: ANSI C code example (finite_AI_ExtClk)
	// The correct order: 
	//(1) StartTask: Scanin, Scanout 
	//(2) StartTask: Counter0
	//(3) ReadAnalogF64: Scanout
	// After (1) and (2), program proceeds without a waiting even if the generation and acquisation are still in progress.
	// Program wait at (3) for it to finish for a maximum duration of timeout

	// wait indefinitely until the task is done
	int taskDone;
	DAQmxErrChk(taskDone = DAQmxWaitUntilTaskDone(taskHandleScanin, -1));

	DAQmxStopTask(taskHandleScanin);
	//		DAQmxStopTask(taskHandleScanout);
	DAQmxStopTask(taskHandleCounter0);

/*	if (compareMode == 1){
		for (linecount = 0; linecount < total_lines_read; linecount++){
			// write scan out into scanoutData
			// S_OUT(VDD_IO) terminal from chip go through level shifter, shifted to VDD_NIDAQ = 5V
			if (anaInputBuffer[linecount] > VDD_NIDAQ * 0.5) {
				scanoutData[linecount] = 1;
			}
			else {
				scanoutData[linecount] = 0;
			}
		}
	}*/
	// Check if scan out is correct
	// use PHI_1 (scaninData[linecount][1]) to sample scanoutData

	// TODO: figure out why scan out samples are offset from scan in by about 21 sampling clock points!
	// DEFAULT DELAY cycles before the first AI sampling clock triggering? 
	// Understand how hard-wired AI (external/ NIDAQ board internal counter generated) sampling clock timing works!
	// route out and probe ai/SamplingClk from PFI7 (configure it as an output!)
	// Also refresh / double-check: chip internal circuit architecture -- scan chain arrangement / length??

/*	if (compareMode == 1) {
		for (linecount = 0; linecount < total_lines_read; linecount++){
			if (scaninData[linecount][1] == 1) {
				if (scanoutData[linecount] != scaninData[linecount][0]){
					SCAN_EQUAL = 0;
					printf("line=%d\n", linecount);
					printf("scanin=%d\n", scaninData[linecount][0]);
					printf("scanout=%d, AI=%f\n", scanoutData[linecount], anaInputBuffer[linecount]);
				}
			}
		}
	}*/

	/*******************************************/
	// Output
	/*******************************************/
	//printf("SCAN OKAY\n");
/*	if (compareMode == 1) {
		printf("SCAN_EQUAL: %d  \n", SCAN_EQUAL);
	}*/

Error:
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
	if (taskHandleScanin != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandleScanin);
		DAQmxClearTask(taskHandleScanin);
	}
	// Andrew added following section for taskHandleScanout
//	if (taskHandleScanout != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
/*		DAQmxStopTask(taskHandleScanout);
		DAQmxClearTask(taskHandleScanout);
	}
	*/
	// I added this for counter out
	if (taskHandleCounter0 != 0){
		DAQmxStopTask(taskHandleCounter0);
		DAQmxClearTask(taskHandleCounter0);
	}
	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);
	fclose(fptr_scanin);
	return 0;
}

int32 CVICALLBACK DoneCallback(TaskHandle taskHandle, int32 status, void *callbackData)
{
	int32   error = 0;
	char    errBuff[2048] = { '\0' };

	// Check to see if an error stopped the task.
	DAQmxErrChk(status);

Error:
	if (DAQmxFailed(error)) {
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
		DAQmxClearTask(taskHandle);
		printf("DAQmx Error: %s\n", errBuff);
	}
	return 0;
}

/*********************** copy and adapted from DO_USB6008 
 * add a delay (in ms) in between mux enable and mux disable
 * only intended for controling the VS/VD connecting time more accurately using USB6008 while PSU output is already enabled *****/
int MUX_Delay_DO_USB6008(char *fn_scanin, int Delay_ms)
{
	//NIDAQ-USB6008, digital output port: "dev2/port0/line0:7"
	//generate timing non-critical signals, including MUX addresses A[0:4]
	// line0, line1, line2, line3, line4,          line5,           line6,         line7,       line8,        line9
	//  A0,    A1,    A2,     A3,   A4,     /EN0=/WR0=/CS0, /EN1=/WR1=/CS1, /EN2=/WR2=/CS2, /EN3=/WR3=/CS3, POLARITY {=0 for write, =1 for erase)
	// (Due to NIDAQ hardware limitation?) use on-demand timing one-sample a time ("implicit timing", software controlled immediate output)

#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else
	// Variables for Error Buffer
	int32 error = 0;
	char errBuff[2048] = { '\0' };


	//Variables for Processing of Input lines (output to chip)
	TaskHandle taskHandleScanin = 0;

	uInt8 scaninData[MAX_LINES][NUM_OUTPUT_USB6008];

	int total_lines_read = 0;

	int linecount = 0;

	/*********************************************/
	//  FILE I/O Intialization
	/*********************************************/

	//file IO variables
	FILE *fptr_scanin;

	if ((fptr_scanin = fopen(fn_scanin, "r")) == NULL){
		printf("Cannot open%s.\n", fn_scanin);
		return FAIL;
	}

	//printf("DEBUG:scanin file found\n");

	/*********************************************/
	// READ vectors from input file
	/*********************************************/
	int input_char;

	scanChainRead_Error = 0;

	//loop for each line of file I/O
	while (((input_char = fgetc(fptr_scanin)) != EOF) && (linecount < MAX_LINES)) {
		//loop to read an input line
		int inputcnt = 0;
		while (input_char != '\n'){
			if (input_char == '1'){
				scaninData[linecount][inputcnt] = 1;
				inputcnt++;
			}
			else if ((input_char == '0') | (input_char == 'x') |
				(input_char == 'X') | (input_char == 'z') |
				(input_char == 'Z')){
				scaninData[linecount][inputcnt] = 0;
				inputcnt++;
			}

			if ((input_char = fgetc(fptr_scanin)) == EOF)
				break;
		}

		if (inputcnt != NUM_OUTPUT_USB6008){
			printf("Input line %d was not formatted correctly, received %d input characters\n", linecount, inputcnt);
			printf("Data: ");
			for (int i = 0; i<NUM_OUTPUT_USB6008; i++)
				printf("%d ", scaninData[linecount][i]);
			printf("\n");
			scanChainRead_Error = 1;
		}
		linecount++;
		//printf("DEBUG: Read line %d\n", linecount);
	}
	fclose(fptr_scanin);
	total_lines_read = linecount;
	//printf("DEBUG: Finished Reading input file\n");

	/*********************************************/
	// DAQmx Configure Code
	/*********************************************/
	DAQmxErrChk(DAQmxCreateTask("OutputTask", &taskHandleScanin));
	DAQmxErrChk(DAQmxCreateDOChan(taskHandleScanin, outchannel_str_USB6008, "outchannel", DAQmx_Val_ChanForAllLines));
	DAQmxCfgImplicitTiming(taskHandleScanin, DAQmx_Val_FiniteSamps, 1);

	/*********************************************/
	// DAQmx Start Code
	/*********************************************/
	//	DAQmxErrChk (DAQmxStartTask(taskHandleScanin));
	//	DAQmxErrChk (DAQmxStartTask(taskHandleScanout));

	/*********************************************/
	// DAQmx Read and Write Code - Loop
	/*********************************************/
	for (linecount = 0; linecount < total_lines_read; linecount++){
		//Write Code
		DAQmxErrChk(DAQmxWriteDigitalLines(taskHandleScanin, 1, 1, 10.0, DAQmx_Val_GroupByChannel, scaninData[linecount], NULL, NULL));
//		::Sleep(Delay_ms); //add this delay in between the 1st and 2nd lines (intended for a 2-line scan file)
	}
	/*******************************************/
	// Output
	/*******************************************/
	//printf("SCAN OKAY\n");

Error:
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
	if (taskHandleScanin != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandleScanin);
		DAQmxClearTask(taskHandleScanin);
	}
	// Andrew added following section for taskHandleScanout

	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);
	fclose(fptr_scanin);
	return 0;
}

int DO_USB6008(char *fn_scanin)
{
	//NIDAQ-USB6008, digital output port: "dev2/port0/line0:7"
	//generate timing non-critical signals, including MUX addresses A[0:4]
	// line0, line1, line2, line3, line4,          line5,           line6,         line7,       line8,        line9
	//  A0,    A1,    A2,     A3,   A4,     /EN0=/WR0=/CS0, /EN1=/WR1=/CS1, /EN2=/WR2=/CS2, /EN3=/WR3=/CS3, POLARITY {=0 for write, =1 for erase)
	// (Due to NIDAQ hardware limitation?) use on-demand timing one-sample a time ("implicit timing", software controlled immediate output)

#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else
	// Variables for Error Buffer
	int32 error = 0;
	char errBuff[2048] = { '\0' };


	//Variables for Processing of Input lines (output to chip)
	TaskHandle taskHandleScanin = 0;

	uInt8 scaninData[MAX_LINES][NUM_OUTPUT_USB6008];

	int total_lines_read = 0;

	int linecount = 0;

	/*********************************************/
	//  FILE I/O Intialization
	/*********************************************/

	//file IO variables
	FILE *fptr_scanin;

	if ((fptr_scanin = fopen(fn_scanin, "r")) == NULL){
		printf("Cannot open%s.\n", fn_scanin);
		return FAIL;
	}

	//printf("DEBUG:scanin file found\n");

	/*********************************************/
	// READ vectors from input file
	/*********************************************/
	int input_char;

	scanChainRead_Error = 0;

	//loop for each line of file I/O
	while (((input_char = fgetc(fptr_scanin)) != EOF) && (linecount < MAX_LINES)) {
		//loop to read an input line
		int inputcnt = 0;
		while (input_char != '\n'){
			if (input_char == '1'){
				scaninData[linecount][inputcnt] = 1;
				inputcnt++;
			}
			else if ((input_char == '0') | (input_char == 'x') |
				(input_char == 'X') | (input_char == 'z') |
				(input_char == 'Z')){
				scaninData[linecount][inputcnt] = 0;
				inputcnt++;
			}

			if ((input_char = fgetc(fptr_scanin)) == EOF)
				break;
		}

		if (inputcnt != NUM_OUTPUT_USB6008){
			printf("Input line %d was not formatted correctly, received %d input characters\n", linecount, inputcnt);
			printf("Data: ");
			for (int i = 0; i<NUM_OUTPUT_USB6008; i++)
				printf("%d ", scaninData[linecount][i]);
			printf("\n");
			scanChainRead_Error = 1;
		}
		linecount++;
		//printf("DEBUG: Read line %d\n", linecount);
	}
	fclose(fptr_scanin);
	total_lines_read = linecount;
	//printf("DEBUG: Finished Reading input file\n");

	/*********************************************/
	// DAQmx Configure Code
	/*********************************************/
	DAQmxErrChk(DAQmxCreateTask("OutputTask", &taskHandleScanin));
	DAQmxErrChk(DAQmxCreateDOChan(taskHandleScanin, outchannel_str_USB6008, "outchannel", DAQmx_Val_ChanForAllLines));
	DAQmxCfgImplicitTiming(taskHandleScanin, DAQmx_Val_FiniteSamps, 1);

	/*********************************************/
	// DAQmx Start Code
	/*********************************************/
	//	DAQmxErrChk (DAQmxStartTask(taskHandleScanin));
	//	DAQmxErrChk (DAQmxStartTask(taskHandleScanout));

	/*********************************************/
	// DAQmx Read and Write Code - Loop
	/*********************************************/
	for (linecount = 0; linecount < total_lines_read; linecount++){
		//Write Code
		DAQmxErrChk(DAQmxWriteDigitalLines(taskHandleScanin, 1, 1, 10.0, DAQmx_Val_GroupByChannel, scaninData[linecount], NULL, NULL));
	}
	/*******************************************/
	// Output
	/*******************************************/
	//printf("SCAN OKAY\n");

Error:
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
	if (taskHandleScanin != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandleScanin);
		DAQmxClearTask(taskHandleScanin);
	}
	// Andrew added following section for taskHandleScanout

	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);
	fclose(fptr_scanin);
	return 0;
}



int scanInternalRead(char *fn_scanin, int *RegsOut)
{
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else

	// Variables for Error Buffer
	int32 error = 0;
	char errBuff[2048] = { '\0' };

	//Variables for Processing of Input lines (output to chip)
	TaskHandle taskHandleScanin = 0;
	TaskHandle taskHandleScanout = 0;
	uInt8 scaninData[MAX_LINES][NUM_OUTPUT];
	int scanoutData[MAX_LINES];
	//	int internalRegs[NUM_SCAN_BITS];

	int total_lines_read = 0;
	int SCAN_EQUAL = 1;
	int ScanOutNumber = 0;

	float64 anaInputBuffer[MAX_LINES];
	int32 read, bytesPerSamp;

	int linecount = 0;

	/*********************************************/
	//  FILE I/O Intialization
	/*********************************************/

	//file IO variables
	FILE *fptr_scanin;

	if ((fptr_scanin = fopen(fn_scanin, "r")) == NULL){
		printf("Cannot open%s.\n", fn_scanin);
		return FAIL;
	}
	//printf("DEBUG:scanin file found\n");

	/*********************************************/
	// READ vectors from input file
	/*********************************************/
	int input_char;

	//loop for each line of file I/O
	while (((input_char = fgetc(fptr_scanin)) != EOF) && (linecount < MAX_LINES)) {
		//loop to read an input line
		int inputcnt = 0;
		while (input_char != '\n'){
			if (input_char == '1'){
				scaninData[linecount][inputcnt] = 1;
				inputcnt++;
			}
			else if ((input_char == '0') | (input_char == 'x') |
				(input_char == 'X') | (input_char == 'z') |
				(input_char == 'Z')){
				scaninData[linecount][inputcnt] = 0;
				inputcnt++;
			}

			if ((input_char = fgetc(fptr_scanin)) == EOF)
				break;
		}

		if (inputcnt != NUM_OUTPUT){
			printf("Input line %d was not formatted correctly, received %d input characters\n", linecount, inputcnt);
			printf("Data: ");
			for (int i = 0; i<NUM_OUTPUT; i++)
				printf("%d ", scaninData[linecount][i]);
			printf("\n");
		}
		linecount++;
		//printf("DEBUG: Read line %d\n", linecount);
	}

	fclose(fptr_scanin);
	total_lines_read = linecount;
	//printf("DEBUG: Finished Reading input file\n");

	/*********************************************/
	// DAQmx Configure Code
	/*********************************************/
	DAQmxErrChk(DAQmxCreateTask("OutputTask", &taskHandleScanin));
	DAQmxErrChk(DAQmxCreateDOChan(taskHandleScanin, outchannel_str, "outchannel", DAQmx_Val_ChanForAllLines));
	DAQmxCfgImplicitTiming(taskHandleScanin, DAQmx_Val_FiniteSamps, 1);

	DAQmxErrChk(DAQmxCreateTask("ReadTask", &taskHandleScanout));
	DAQmxErrChk(DAQmxCreateAIVoltageChan(taskHandleScanout, inchannel_str, "inchannel", DAQmx_Val_Cfg_Default, 0.0, 10.0, DAQmx_Val_Volts, NULL));

	/*********************************************/
	// DAQmx Start Code
	/*********************************************/
	DAQmxErrChk(DAQmxStartTask(taskHandleScanin));
	DAQmxErrChk(DAQmxStartTask(taskHandleScanout));

	/*********************************************/
	// DAQmx Read and Write Code - Loop
	/*********************************************/

	for (linecount = 0; linecount<total_lines_read; linecount++){
		//Write Code
		DAQmxErrChk(DAQmxWriteDigitalLines(taskHandleScanin, 1, 1, 10.0, DAQmx_Val_GroupByChannel, scaninData[linecount], NULL, NULL));
		//Read Signals
		DAQmxErrChk(DAQmxReadAnalogF64(taskHandleScanout, 1, 10.0, DAQmx_Val_GroupByChannel, anaInputBuffer, 1, &read, NULL));

		// write scan out into scanoutData
		if (anaInputBuffer[0]>2) {
			scanoutData[linecount] = 1;
		}
		else {
			scanoutData[linecount] = 0;
		}

		// Use clk2 to Store Internal Register Values to Array
		if (scaninData[linecount][1] == 1) {
			RegsOut[9067 - ScanOutNumber] = scanoutData[linecount];
			ScanOutNumber = ScanOutNumber + 1;
		}
	}
	printf("Read Out Scan Bit %d\n", ScanOutNumber);
	//	printf("Reading Out Scan Done\n");
	/*******************************************/
	// Output
	/*******************************************/
	printf("SCAN OKAY\n");

Error:
	if (DAQmxFailed(error))
		DAQmxGetExtendedErrorInfo(errBuff, 2048);
	if (taskHandleScanin != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandleScanin);
		DAQmxClearTask(taskHandleScanin);
	}
	// Andrew added following section for taskHandleScanout
	if (taskHandleScanout != 0) {
		/*********************************************/
		// DAQmx Stop Code
		/*********************************************/
		DAQmxStopTask(taskHandleScanout);
		DAQmxClearTask(taskHandleScanout);
	}

	if (DAQmxFailed(error))
		printf("DAQmx Error: %s\n", errBuff);
	fclose(fptr_scanin);
	return 0;
}

int scanFileChk(char *fn_scanin)
{
#define DAQmxErrChk(functionCall) if( DAQmxFailed(error=(functionCall)) ) goto Error; else
	// Variables for Error Buffer
	int32 error = 0;
	char errBuff[2048] = { '\0' };

	//Variables for Processing of Input lines (output to chip)
	uInt8 scaninData[MAX_LINES][NUM_OUTPUT];
	int scanoutData[MAX_LINES];

	int total_lines_read = 0;

	int linecount = 0;

	/*********************************************/
	//  FILE I/O Intialization
	/*********************************************/

	//file IO variables
	FILE *fptr_scanin;

	if ((fptr_scanin = fopen(fn_scanin, "r")) == NULL){
		printf("Cannot open%s.\n", fn_scanin);
		return FAIL;
	}

	//printf("DEBUG:scanin file found\n");

	/*********************************************/
	// READ vectors from input file
	/*********************************************/
	int input_char;

	scanChainRead_Error = 0;

	//loop for each line of file I/O
	while (((input_char = fgetc(fptr_scanin)) != EOF) && (linecount < MAX_LINES)) {
		//loop to read an input line
		int inputcnt = 0;
		while (input_char != '\n'){
			if (input_char == '1'){
				scaninData[linecount][inputcnt] = 1;
				inputcnt++;
			}
			else if ((input_char == '0') | (input_char == 'x') |
				(input_char == 'X') | (input_char == 'z') |
				(input_char == 'Z')){
				scaninData[linecount][inputcnt] = 0;
				inputcnt++;
			}

			if ((input_char = fgetc(fptr_scanin)) == EOF)
				break;
		}

		if (inputcnt != NUM_OUTPUT){
			printf("Input line %d was not formatted correctly, received %d input characters\n", linecount, inputcnt);
			printf("Data: ");
			for (int i = 0; i<NUM_OUTPUT; i++)
				printf("%d ", scaninData[linecount][i]);
			printf("\n");
			scanChainRead_Error = 1;
		}
		linecount++;
		//printf("DEBUG: Read line %d\n", linecount);
	}
	fclose(fptr_scanin);
	total_lines_read = linecount;

	if (total_lines_read != 36276) {
		scanChainRead_Error = 1;
	}
	//printf("DEBUG: Finished Reading input file\n");

	return 0;
}
