#ifndef _ENTRY_CPP
#define _ENTRY_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_DEPRECATE

#include <Windows.H>
#include <ShlObj.H>
#include <Stdio.H>
#include <Stdlib.H>

#include "../Resources/Resource.H"

#include "Entry.H"
#include "Init.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BUFFERSZ (1024 * 1024)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HINSTANCE ghAppInstance = NULL;

char gsAppCopyRight[64];
char gsFileVersion[32];
char gsTitleCaption[64];

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct CONFIGINFO_TAG
{
	bool bConvertUnicode;
	int iLinesToSkip;
	char **sFile;
	char sTarget[MAX_PATH];
	int iFiles;
	bool bSilent;
	bool bAppend;

} CONFIGINFO, *LPCONFIGINFO;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ParseCmdLine(int iArgs, char *sArgs[], LPCONFIGINFO lpCI);
bool FreeConfigInfo(LPCONFIGINFO lpCI);
int ProcessJob(LPCONFIGINFO lpCI);
void WriteHelp(void);
int GetFlagedValue(const char *sBuf, int iSkpLen, int iBufSz, char *sOutBuf);
int CompareFlagedString(const char *sBuf, const char *sFlag);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int iArgs, char *sArgs[])
{
	int iResult = 0;

	ghAppInstance = GetModuleHandle(NULL);

	if(!InitializeApp())
    {
		printf("Failed to initialize the appication.");
		return 1;
    }

	CONFIGINFO CI;
	if(ParseCmdLine(iArgs, sArgs, &CI))
	{
		iResult = ProcessJob(&CI);
		FreeConfigInfo(&CI);
	}

	if(!UninitializeApp())
    {
        return 1;
    }

	return iResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WriteHelp(void)
{
	printf("\n%s - %s\n", gsTitleCaption, gsFileVersion);
	printf("%s\n\n", gsAppCopyRight);

	printf("Syntax:\n");
	printf(" CombineFiles [/Target:...] [/Skip:...] [/Append] [/Silent] [Source files...]\n");

	printf("  /?          - Displays this help message.\n");
	printf("  /Target:    - Defines the target file that will written to.\n");
	printf("  /Skip:      - Defines the top number of lines to skip within each source file.\n");
	printf("  /Append     - Tells the application to append to the target file.\n");
	printf("  /Silent     - Tells the application not to print verbose information.\n");
	printf("  /CvtUnicode - Converts any unicode files to ascii before merging.\n");
	printf("\n");

	printf(" Example:\n");
	printf("  CombineFiles /Target:FY08.txt /Append FY06.txt FY07.txt\n");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int ProcessJob(LPCONFIGINFO lpCI)
{
	FILE *hTarget = NULL;
	FILE *hSource = NULL;

	char *sWriteMode = NULL;

	int iBytesRead = 0;
	int iErrorCount = 0;

	if(lpCI->bAppend)
	{
		if(!lpCI->bSilent)
		{
			printf("Opening [%s]\n", lpCI->sTarget);
		}
		sWriteMode = "ab";
	}
	else{
		if(!lpCI->bSilent)
		{
			printf("Creating [%s]\n", lpCI->sTarget);
		}
		sWriteMode = "wb";
	}

	if((hTarget = fopen(lpCI->sTarget, sWriteMode)) == NULL)
	{
		printf("Failed to open the target file."
			" Do you have permission to access this file?\n[%s]\n", lpCI->sTarget);
		iErrorCount++;
		return iErrorCount;
	}

	char *sBuf = (char *)calloc(sizeof(char), BUFFERSZ + 1);
	char *sUnicodeBuf = NULL;

	for(int iItem = 0; iItem < lpCI->iFiles; iItem++)
	{
		bool bSkipFile = false;

		if(!lpCI->bSilent)
		{
			printf("Appending [%s]\n", lpCI->sFile[iItem]);
		}

		if((hSource = fopen(lpCI->sFile[iItem], "rb")) == NULL)
		{
			printf("Failed to open the source file. Does the file exist?\n[%s]", lpCI->sTarget);
			free(sBuf);
			iErrorCount++;
			return iErrorCount;
		}

		int iSkipped = 0;

		while(iSkipped < lpCI->iLinesToSkip)
		{
			if(!fgets(sBuf, BUFFERSZ, hSource))
			{
				printf("File contains no data after skipping %d line(s)\n[%s]\n", iSkipped + 1, lpCI->sTarget);
				bSkipFile = true;
				iErrorCount++;
				break;
			}
			iSkipped++;
		}

		if(!bSkipFile)
		{
			do{
				iBytesRead = fread(sBuf, sizeof(char), BUFFERSZ, hSource);

				if(lpCI->bConvertUnicode)
				{
					if(IsTextUnicode(sBuf, iBytesRead, NULL))
					{
						if(sUnicodeBuf == NULL)
						{
							sUnicodeBuf = (char *)calloc(sizeof(char), BUFFERSZ + 1);
						}

						iBytesRead = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR) sBuf, -1, sUnicodeBuf, iBytesRead, " ", NULL);
						memcpy(sBuf, sUnicodeBuf, iBytesRead);
					}
				}

				if(fwrite(sBuf, sizeof(char), iBytesRead, hTarget) != iBytesRead)
				{
					printf("Failed to write data to the target file. Out of space?\n[%s]\n", lpCI->sTarget);
					iErrorCount++;
				}
			}
			while(iBytesRead == BUFFERSZ);
		}

		fclose(hSource);
	}

	free(sBuf);
	fclose(hTarget);

	return iErrorCount;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ParseCmdLine(int iArgs, char *sArgs[], LPCONFIGINFO lpCI)
{
	int iArg = 0;
	int iArgSz = 0;
	int iParamSz = 0;
	char *sArg = NULL;

	char sTemp[5120];

	memset(lpCI, 0, sizeof(CONFIGINFO));

	if(iArgs <= 1)
	{
		WriteHelp();
		return false;
	}

	for(iArg = 1; iArg < iArgs; iArg++)
	{
		iParamSz = strlen(sArgs[iArg]);

		sArg = "/?";
		iArgSz = strlen(sArg);
		if(_strnicmp(sArgs[iArg], sArg, iArgSz) == 0)
		{
			WriteHelp();
			return false;
		}		

		sArg = "-?";
		iArgSz = strlen(sArg);
		if(_strnicmp(sArgs[iArg], sArg, iArgSz) == 0)
		{
			WriteHelp();
			return false;
		}		

		sArg = "/Skip:";
		iArgSz = strlen(sArg);
		if(_strnicmp(sArgs[iArg], sArg, iArgSz) == 0)
		{
			GetFlagedValue(sArgs[iArg], iArgSz, iParamSz, sTemp);
			lpCI->iLinesToSkip = atol(sTemp);
			continue;
		}

		sArg = "/Append";
		iArgSz = strlen(sArg);
		if(_strnicmp(sArgs[iArg], sArg, iArgSz) == 0)
		{
			lpCI->bAppend = true;
			continue;
		}

		sArg = "/CvtUnicode";
		iArgSz = strlen(sArg);
		if(_strnicmp(sArgs[iArg], sArg, iArgSz) == 0)
		{
			lpCI->bConvertUnicode = true;
			continue;
		}		
		
		sArg = "/Silent";
		iArgSz = strlen(sArg);
		if(_strnicmp(sArgs[iArg], sArg, iArgSz) == 0)
		{
			lpCI->bSilent = true;
			continue;
		}		
		
		sArg = "/Target:";
		iArgSz = strlen(sArg);
		if(_strnicmp(sArgs[iArg], sArg, iArgSz) == 0)
		{
			GetFlagedValue(sArgs[iArg], iArgSz, iParamSz, lpCI->sTarget);
			continue;
		}

		//Save the file name in the array.
		iArgSz = (iParamSz + 1);
		lpCI->sFile = (char **) realloc(lpCI->sFile, (sizeof(char *) * (lpCI->iFiles + 1)));
		lpCI->sFile[lpCI->iFiles] = (char *) calloc(sizeof(char), iParamSz + 1);
		strcpy_s(lpCI->sFile[lpCI->iFiles], iArgSz, sArgs[iArg]);
		lpCI->iFiles++;
	}

	if(strlen(lpCI->sTarget) == 0)
	{
		printf("No target file was specified.\n");
		return false;
	}

	if(lpCI->iFiles == 0)
	{
		printf("No source files were specified.\n");
		return false;
	}

	if(lpCI->iLinesToSkip < 0)
	{
		printf("The specified number of lines to skip is less than 0.\n");
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FreeConfigInfo(LPCONFIGINFO lpCI)
{
	if(lpCI->iFiles > 0)
	{
		for(int iItem = 0; iItem < lpCI->iFiles; iItem++)
		{
			free(lpCI->sFile[iItem]);
			lpCI->sFile[iItem] = NULL;
		}

		free(lpCI->sFile);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CompareFlagedString(const char *sBuf, const char *sFlag)
{
	int iFlagLength = strlen(sFlag);

	if( strncmp(sBuf, sFlag, iFlagLength) == 0)
	{
		return iFlagLength;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int GetFlagedValue(const char *sBuf, int iSkpLen, int iBufSz, char *sOutBuf)
{
	int iWPos = 0;
	int iRPos = iSkpLen;

	while(iRPos < iBufSz)
	{
		sOutBuf[iWPos++] = sBuf[iRPos++];
	}

	sOutBuf[iWPos] = '\0';

	return iWPos; //The length of the string returned in sOutBuf.
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
