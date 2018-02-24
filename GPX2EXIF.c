#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<dirent.h>

#define TIMEZONE 9
#define MAX_FILE_COUNT 512
#define MAX_POINT_COUNT 131070

#define WORKSPACE "/Volumes/WorkSpace"

struct _time
{
	int year;
	int month;
	int date;
	int hour;
	int minute;
	int second;
};

struct _point
{
	struct _time time;

	// 0 is North, 1 is South.
	int  latitudeRef;
	char latitude[16];

	// 0 is East, 1 is West.
	int  longitudeRef;
	char longitude[16];
	char elevation[16];
};

int GetLatLon(char latlon[], char line[], char *rtn)
{
	int index;
	int start;
	
	// 5 is length of "lat=\""
	start = strstr(line, latlon) - line + 5;
	for (index = 0; line[start + index] != '"'; index++)
		rtn[index] = line[start + index];
	rtn[index] = '\0';

	if (atof(rtn) < 0)
		return 1;
	else
		return 0;
}

int Get2Time(char line[], int offset)
{
	char str[3];

	str[0] = line[offset];
	str[1] = line[offset + 1];
	str[2] = '\0';

	return atoi(str);
}

int Get4Time(char line[], int offset)
{
	int index;
	char str[5];

	str[0] = line[offset];
	str[1] = line[offset + 1];
	str[2] = line[offset + 2];
	str[3] = line[offset + 3];
	str[4] = '\0';

	return atoi(str);
}

void GetTime(char line[], struct _point *pt)
{
	int n_of_date;
	char *position;
	int offset;

	if ((position = strstr(line, "<time>")) != NULL)
		offset = position - line;
	else
		printf("Where is my clock.");
	
	pt -> time.year   = Get4Time(line, offset + 6);
	pt -> time.month  = Get2Time(line, offset + 11);
	pt -> time.date   = Get2Time(line, offset + 14);
	pt -> time.hour   = Get2Time(line, offset + 17) + TIMEZONE;
	pt -> time.minute = Get2Time(line, offset + 20);
	pt -> time.second = Get2Time(line, offset + 23);

	if (pt -> time.hour > 23)
	{
		pt -> time.hour -= 24;
		pt -> time.date++;
	}

	switch (pt -> time.month)
	{
	case 2:
		if (((pt -> time.year % 4) == 0 && (pt -> time.year % 100) != 0) || (pt -> time.year % 400) == 0)
			n_of_date = 29;
		else
			n_of_date = 28;
		break;
	case 1: case 3: case 5: case 7: case 8: case 10: case 12:
		n_of_date = 31;
		break;
	case 4: case 6: case 9: case 11:
		n_of_date = 30;
		break;
	default:
		printf("ERROR\n");
		printf("What date is it today?\n\n");
		exit(1);
	}
	
	if (pt -> time.date > n_of_date)
	{
		pt -> time.month++;
		pt -> time.date = n_of_date;
	}
}

void GetElevation(char line[], struct _point *pt)
{
	int index;
	int start;
	
	start = strstr(line, "<ele>") - line + 5;
	
	for (index = 0; line[start + index] != '<'; index++)
		pt -> elevation[index] = line[start + index];
	pt -> elevation[index] = '\0';
}

void GetPoint(FILE *fp, char line[], struct _point *pt[], int ptIndex)
{
	pt[ptIndex] = malloc(sizeof(struct _point));
	
	pt[ptIndex] -> latitudeRef  = GetLatLon("lat", line, pt[ptIndex] -> latitude);
	pt[ptIndex] -> longitudeRef = GetLatLon("lon", line, pt[ptIndex] -> longitude);

	while (fgets(line, 256, fp))
	{
		line[strlen(line) - 1] = '\0';
		
		if (strstr(line, "</trkpt>") != NULL)
			break;
		else if (strstr(line, "<time>") != NULL)
			GetTime(line, pt[ptIndex]);
		else if (strstr(line, "<ele>") != NULL)
			GetElevation(line, pt[ptIndex]);
	}
}

int GPXOpen(char *fileName, struct _point *pt[])
{
	int ptIndex = 0;
	char line[256];
	FILE *fp;
	
	fp = fopen(fileName, "r");
	while (fgets(line, 256, fp) != NULL)
	{
		if (strstr(line, "</gpx>") != NULL)
			break;
		else if (strstr(line, "<trkpt") != NULL)
			GetPoint(fp, line, pt, ptIndex++);
	}
	fclose(fp);

	return ptIndex;
}

void ShowPoint(struct _point *pt, int pointIndex)
{
	printf("--%04d--------------------", pointIndex);
	printf("\n");
	printf("%d/%02d/%02d ", pt -> time.year, pt -> time.month, pt -> time.date);
	printf("%02d:%02d:%02d", pt -> time.hour, pt -> time.minute, pt -> time.second);
	printf("\n");
	printf("elevation : %s", pt -> elevation);
	printf("\n");
	printf("latitude : %s", pt -> latitude);
	printf("\n");
	printf("longitude : %s", pt -> longitude);
	printf("\n");
}

int FindNearSecond(int second, struct _point *pt[], int ptMinute, int index)
{
	int i;
	int diff;
	int min = 60;
	int minIndex = -1;

	// one minute ago
	diff = abs(pt[index] -> time.second - second - 60);
	if (min > diff)
	{
		min = diff;
		minIndex = index - 1;
	}

	for (i = 0; pt[index + i] -> time.minute == ptMinute; i++)
	{
		diff = abs(pt[index + i] -> time.second - second);
		if (min > diff)
		{
			min = diff;
			minIndex = index + i;
		}
	}

	// next one minute
	diff = abs(pt[index + i] -> time.second - second + 60);
	if (min > diff)
	{
		min = diff;
		minIndex = index + i;
	}

	return minIndex;
}

int FindNearPoint(struct _time time, struct _point *pt[], int ptIndex)
{
	int index;

	for (index = 0; index < ptIndex; index++)
	{
		if (pt[index] -> time.year == time.year)
			if (pt[index] -> time.month == time.month)
				if (pt[index] -> time.date == time.date)
					if (pt[index] -> time.hour == time.hour)
						if (pt[index] -> time.minute == time.minute)
							return FindNearSecond(time.second, pt, pt[index] -> time.minute, index);
	}

	return -1;
}

void GetFileList(char *fileList[])
{
	int index;
	DIR *dir;
	struct dirent *dirst;
	char *d_name;

	dir = opendir(WORKSPACE);
	if (dir == NULL)
	{
		printf("I can't open directry, \"%s\"\n", WORKSPACE);
		exit(1);
	}
	
	index = 0;
	while((dirst = readdir(dir)) != NULL)
	{
		d_name = dirst -> d_name;
		if (d_name[0] == '.')
		{}
		else if (strstr(d_name, ".PEF_original"))
		{}
		else if (strstr(d_name, ".PEF"))
		{
			fileList[index] = (char *)malloc(strlen(d_name));
			strcpy(fileList[index], d_name);
			fileList[index][strlen(d_name)] = '\0';

			index++;
		}
		else if (strstr(d_name, ".JPG"))
		{
			fileList[index] = (char *)malloc(strlen(d_name));
			strcpy(fileList[index], d_name);
			fileList[index][strlen(d_name)] = '\0';

			index++;
		}
	}
	closedir(dir);
}

struct _time GetPhotoTime(char *file)
{
	char main[256] = {"exiftool -DateTimeOriginal "};
	char end[] = {" > TempGPX2EXIF"};

	FILE *fp;
	char str[256];
	char tmp[5];

	struct _time time;
	
	//strcat(main, pre);
	strcat(main, WORKSPACE);
	strcat(main, "/");
	strcat(main, file);
	strcat(main, end);
	system(main);

	fp = fopen("TempGPX2EXIF", "r");
	fgets(str, 256, fp);

	time.year   = Get4Time(str, 34);
	time.month  = Get2Time(str, 39);
	time.date   = Get2Time(str, 42);
	time.hour   = Get2Time(str, 45);
	time.minute = Get2Time(str, 48);
	time.second = Get2Time(str, 51);

	fclose(fp);

	return time;
}

void SetGPSTag(char *fileName, struct _point *pt)
{
	char command[256] = "exiftool ";
	
	struct _time time;

	strcat(command, "-GPSLatitude=");
	strcat(command, "\"");
	strcat(command, pt -> latitude);
	strcat(command, "\" ");
	strcat(command, "-GPSLongitude=");
	strcat(command, "\"");
	strcat(command, pt -> longitude);
	strcat(command, "\" ");

	strcat(command, "-GPSLatitudeRef=");
	strcat(command, "\"");
	if (pt -> latitudeRef == 0)
		strcat(command, "North");
	else
		strcat(command, "South");
	strcat(command, "\" ");
	
	strcat(command, "-GPSLongitudeRef=");
	strcat(command, "\"");
	if (pt -> longitudeRef == 0)
		strcat(command, "East");
	else
		strcat(command, "West");
	strcat(command, "\" ");

	strcat(command, "-GPSAltitude=");
	strcat(command, "\"");
	strcat(command, pt -> elevation);
	strcat(command, "\" ");

	strcat(command, WORKSPACE);
	strcat(command, "/");
	strcat(command, fileName);

	strcat(command, " > /dev/null");
	
	system(command);
}

int main(int argc, char *argv[])
{
	int index;
	int neartime;
	int notFindIndex = 0;
	int notfind[MAX_FILE_COUNT];
	
	int ptIndex;
	struct _point *pt[MAX_POINT_COUNT] = {NULL};
	struct _time photoTime;
	char *fileList[MAX_FILE_COUNT] = {NULL};

	// Open GPX file.
	ptIndex = GPXOpen(argv[1], pt);
	
	// Get all photo-file-name.
	GetFileList(fileList);

	// Get Photo's-time-stamp.
	for (index = 0; fileList[index] != NULL; index++)
	{
		printf("%s\n", fileList[index]);
		photoTime = GetPhotoTime(fileList[index]);
		neartime = FindNearPoint(photoTime, pt, ptIndex);

		if (neartime == -1)
		{
			notfind[notFindIndex++] = index;
		}
		else
		{
			SetGPSTag(fileList[index], pt[neartime]);
			printf("%s\t%d\r", fileList[index], ptIndex);
		}
	}

	printf("\n");
	if (notFindIndex == 0)
		printf("I got it.\n\n");
	else if (notFindIndex == 1)
	{
		printf("I can NOT find this photo's GPX-Point.\n");
		printf("%s\n\n", fileList[notfind[0]]);
	}
	else
	{
		printf("I can NOT find these photo's GPX-Points.\n");
		for (index = 0; index < notFindIndex; index++)
			printf("%s\n", fileList[notfind[index]]);
	}
	
	for (index = 0; fileList[index] != NULL; index++)
		free(fileList[index]);

	for (index = 0; index < ptIndex; index++)
		free(pt[index]);
	
	return 0;
}
