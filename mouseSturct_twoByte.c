#include <windows.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "rs232.h"	
#include "rs232.c"	
#include <pthread.h>
#include <cursor.h>
#include <conio.h>

#ifdef _WIN32
	#include <Windows.h>
#else
	#include <unistd.h>
#endif

#define ENTER 13

#define SHOW 1
#define SEND 2
#define SHOWADD 3
#define MOVE 4
#define WARNTOO 5
#define WARNNO 6
#define SIGHT 7
#define BUSY 8
#define NONBUSY 9

#define MAXPAR 6


HANDLE hStdin; 
DWORD fdwSaveOldMode, fdwMode;

VOID ErrorExit(LPSTR);
VOID KeyEventProc(KEY_EVENT_RECORD); 
VOID MouseEventProc(MOUSE_EVENT_RECORD); 
VOID ResizeEventProc(WINDOW_BUFFER_SIZE_RECORD); 

typedef struct{
	char name[20];
	int *F;
	int length;
	float max;
	float min;
	float R;
	float Rot[3];
	float Pos[3];
}MeshList;

MeshList list[10];

typedef struct{
	float M;
	float E;
	float Rot[3];
}Sight;

Sight eye;

void* child(void* data);
void Frame();
void* Show(void* data);
void Send(int len);
void UpView(Sight sight); 
void Read(int len, char *ch, int n);
void ADDFile();
void BuildFile(int point);
void DeleteMyFile(int level);
void ChangeVal(bool add, int level, int num, int base);

char mode[]={'8','N','1','\0'};
int  cport_nr=4,        /* /dev/ttyS0 (COM1 on windows) */
     bdrate=115200;       /* 9600 baud */
     
int i, j, k;
int len, level, num, base, ShowFlag;
float gap[4] = {0.01, 0.1, 1.0, 10.0};
bool SendFlag, PressFlag, UpSightFlag, wait;
POINT Press;

 
int main(VOID) 
{ 
    DWORD cNumRead, i; 
    INPUT_RECORD irInBuf[128]; 
    
    if(RS232_OpenComport(cport_nr, bdrate, mode)){
		printf("Can not open comport\n");
	    exit(0);
	}
 
    // Get the standard input handle. 
 
    hStdin = GetStdHandle(STD_INPUT_HANDLE); 
    if (hStdin == INVALID_HANDLE_VALUE) 
        ErrorExit("GetStdHandle"); 
 
    // Save the current input mode, to be restored on exit. 
 
    if (! GetConsoleMode(hStdin, &fdwSaveOldMode) ) 
        ErrorExit("GetConsoleMode"); 

    // Enable the window and mouse input events. 
 
    fdwMode = fdwMode = ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
    if (! SetConsoleMode(hStdin, fdwMode) ) 
        ErrorExit("SetConsoleMode"); 
 
    // Loop to read and handle the next 100 input events. 
	len = -1;
 	base = 2;
 	level = num = 0;
 	SendFlag = PressFlag = UpSightFlag  = wait = false;
 	
 	eye.M = 60.0;
 	eye.E = 500.0;
	eye.Rot[0] = eye.Rot[1] = eye.Rot[2] = 0.0;
	
	pthread_t pt;
	pthread_create(&pt, NULL, child, NULL);
	pthread_t sh;
	pthread_create(&sh, NULL, Show, NULL);
	  
	Frame();
	ShowFlag = SHOWADD;
 	Sleep(2);
 	ShowFlag = SIGHT;
 	Sleep(2);
    while (1){ 
        // Wait for the events. 
 
        if (! ReadConsoleInput( 
                hStdin,      // input buffer handle 
                irInBuf,     // buffer to read into 
                128,         // size of read buffer 
                &cNumRead) ) // number of records read 
            ErrorExit("ReadConsoleInput"); 
 
        // Dispatch the events to the appropriate handler. 

        for (i = 0; i < cNumRead; i++){
            switch(irInBuf[i].EventType){ 
                case KEY_EVENT: // keyboard input 
                    KeyEventProc(irInBuf[i].Event.KeyEvent); 
                    break; 
 
                case MOUSE_EVENT: // mouse input 
                    MouseEventProc(irInBuf[i].Event.MouseEvent); 
                    break; 
 
                case WINDOW_BUFFER_SIZE_EVENT: // scrn buf. resizing 
                    ResizeEventProc( irInBuf[i].Event.WindowBufferSizeEvent ); 
                    break; 
 
                case FOCUS_EVENT:  // disregard focus events 
 
                case MENU_EVENT:   // disregard menu events 
                    break; 
 
                default: 
                    ErrorExit("Unknown event type"); 
                    break; 
            } 
        }
    } 

    // Restore input mode on exit.

    SetConsoleMode(hStdin, fdwSaveOldMode);
 
    return 0; 
}

VOID ErrorExit (LPSTR lpszMessage) 
{ 
    fprintf(stderr, "%s\n", lpszMessage); 

    // Restore input mode on exit.

    SetConsoleMode(hStdin, fdwSaveOldMode);

    ExitProcess(0); 
}

VOID KeyEventProc(KEY_EVENT_RECORD ker){
	if(ker.bKeyDown){
		if(ker.uChar.AsciiChar=='a'){
			ADDFile();
		}
		else if(len!=-1){	
			switch (ker.wVirtualKeyCode) {
			      case VK_LEFT:   if(num>0)num--; else num = MAXPAR; break;
			      case VK_RIGHT:  if(num<MAXPAR)num++; else num = 0; break;
			      case VK_UP:     if(level>0)level--; else level = len; break;
			      case VK_DOWN:   if(level<len)level++; else level = 0; break;
			      default:	{
							switch(ker.uChar.AsciiChar){
								case 'b': 	{	if(base<3)base++; 
												else base = 0;
											}break;
								case '+':	ChangeVal(true, level, num, base); break;
								case '-':	ChangeVal(false, level, num, base);break;
							}
						}break;					
			}ShowFlag = MOVE;
		}
	}
	Sleep(100);
}

VOID MouseEventProc(MOUSE_EVENT_RECORD mer)
{
#ifndef MOUSE_HWHEELED
#define MOUSE_HWHEELED 0x0008
#endif
    POINT p;
    GetCursorPos(&p);
    /*cursor(0, 10);
    gotoxy(0, 10);*/
    
    int x = mer.dwMousePosition.X;
    int y = mer.dwMousePosition.Y;
	//printf("%5d %5d %5d %5d", mer.dwMousePosition.X, mer.dwMousePosition.Y, x, y);
	
    switch(mer.dwEventFlags)
    {
        case 0:{
	            if(mer.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED){  	
	               if(y>0 && y<len+2 && x>20 && x<106){
	            		level = y-1;
	            		if(x<28) num = 0;
	            		else if(x<39) num = 1;
	            		else if(x<47) num = 2;
	            		else if(x<55) num = 3;
	            		else if(x<70) num = 4;
	            		else if(x<78) num = 5;
	            		else if(x<86) num = 6;
	            		else DeleteMyFile(level);
	            		ShowFlag = MOVE;
				   }
				   else if(y==len+2 && x<6){
				   		ADDFile();
				   }
				   else if(y>20){
						PressFlag = true;
						Press.x = p.x;
						Press.y = p.y;
				   }
	            }
	            else if(mer.dwButtonState == RIGHTMOST_BUTTON_PRESSED){
	                //printf("right button press \n");
	            }
	            else {
	            	PressFlag = false;
	            }
            }break;
        case DOUBLE_CLICK:
            //printf("double click\n");
            break;
        case MOUSE_HWHEELED:
            //printf("horizontal mouse wheel\n");
            break;
        case MOUSE_MOVED:{
				if(PressFlag==true){
					int dx = (p.x-Press.x>0)? p.x-Press.x : Press.x - p.x;
               		int dy = (p.y-Press.y>0)? p.y-Press.y : Press.y - p.y;
                	if(dx>dy){
                		eye.Rot[1] += (float)(p.x-Press.x)/2000.0;
                		if(eye.Rot[1]>360.0) eye.Rot[1] -= 360.0;
                		else if (eye.Rot[1]<-360.0) eye.Rot[1] += 360.0;
					}
					else {
						eye.Rot[0] -= (float)(p.y-Press.y)/200.0;
						if(eye.Rot[0]>360.0) eye.Rot[0] -= 360.0;
                		else if (eye.Rot[2]<-360.0) eye.Rot[0] += 360.0;
					}
					UpSightFlag = true;
					ShowFlag = SIGHT;
					Sleep(2);
				}	
			}break;
        case MOUSE_WHEELED:{
        		bool flag;
				if(y>0 && y<len+2 && x>20 && x<90){
					if(mer.dwButtonState>>31==0) flag = true;
					else flag = false;
            		level = y-1;
            		if(x<28) num = 0;
            		else if(x<39) num = 1;
            		else if(x<47) num = 2;
            		else if(x<55) num = 3;
            		else if(x<70) num = 4;
            		else if(x<78) num = 5;
            		else  num = 6;
            		ChangeVal(flag, level, num, base);
			  	}
			  	else if(y==21){
			  		if(mer.dwButtonState>>31==0) {
			  			if(x<6.0) eye.M += 1.0;
			  			else if(x<15.0) eye.E += 1.0;
					}
					else {
						if(x<6.0) eye.M -= 1.0;
			  			else if(x<15.0) eye.E -= 1.0;	
					}
					UpSightFlag = true;
					ShowFlag = SIGHT;
					Sleep(2);
				}
			}break;
        default:
            //printf("unknown\n");
            break;
    }
    ShowFlag = MOVE;
}

VOID ResizeEventProc(WINDOW_BUFFER_SIZE_RECORD wbsr){
    //printf("Resize event\n");
    //printf("Console screen buffer is %d columns by %d rows.\n", wbsr.dwSize.X, wbsr.dwSize.Y);
}

void ADDFile(){
	char str[20];
	FILE *stl;
	SetConsoleMode(hStdin, fdwSaveOldMode);
	if(len<9){
		cursor(1, 100);
		gotoxy(5, len+2);
		setcolor(WHITE);
		scanf("%s", str);
		strcat(str, ".txt");
		stl = fopen(str, "r");
		if(stl!=NULL){
			len++;
			level = len;
			strcpy(list[len].name, str);
			list[len].F = NULL;
			list[len].max = 0.0;
			list[len].min = 1000.0;
			BuildFile(len);
			list[len].R = 10.0;				
			for(i=0;i<3;i++){
				list[len].Pos[i] = 0.0;
				list[len].Rot[i] = 0.0;
			}
			ShowFlag = SHOW;
			Sleep(2);
			ShowFlag = SHOWADD; 
			Sleep(2);
			ShowFlag = MOVE; 
			SendFlag = true;	
		}
		else {
 			ShowFlag = WARNNO;
 			wait = true;
			while(wait);
		}
		fclose(stl);
	}
	else {
		ShowFlag = WARNTOO;
		wait = true;
		while(wait);
	}
	SetConsoleMode(hStdin, fdwMode);
}

void BuildFile(int point){
	FILE *myFile;	
	char str[500];
	int max;
	float loc;
	float *temploc;
	
	myFile = fopen(list[point].name, "r");
	if(myFile == NULL)perror("error");
	temploc = (float *)malloc(900*sizeof(float));
	i = 0;
	max = 900;
	while(1){
	    fscanf(myFile, "%s", str);
	    if(feof(myFile))break;
		if(strcmp(str, "vertex") == 0){
			for(k=0;k<3;k++){
	        	fscanf(myFile, "%s", str);
	        	sscanf(str, "%f", &loc);
	        	if(loc>list[point].max) list[point].max = loc;
	        	else if(loc<list[point].min) list[point].min = loc;
	        	temploc[i] = loc;
	        	i++;
	        	if(i==max){
	        		max += 900;
	        		float *temp = realloc(temploc, max*sizeof(float));
	        		temploc = temp;
				}
			}				
		}
	}
	list[point].length = i;
	list[point].F = (int *)malloc((list[point].length-1)*sizeof(int));
	float range = list[point].max - list[point].min;
	for(i=0;i<list[point].length;i++){
		list[point].F[i] = (int)((temploc[i]-list[point].min)*65534.0/range);
	}
	free(temploc);
	fclose(myFile);
}

void DeleteMyFile(int ll){
	for(i=ll;i<len;i++){
		list[i] = list[i+1];
		level = i;
		ShowFlag = SHOW;
		Sleep(2);
	}
	level = 0;
	len--;
	ShowFlag = SHOWADD; 
	wait = true;
	while(wait);
	SendFlag = true;
}

void ChangeVal(bool add, int level, int num, int base){
	if(add){
		switch(num){
			case 0: list[level].R += gap[base]; break;
			case 1: 
			case 2:
			case 3:	list[level].Rot[num-1] += gap[base]; if(list[level].Rot[num-1]>360.0) list[level].Rot[num-1] = 0.0; break;
			case 4:
			case 5: 
			case 6: list[level].Pos[num-4] += gap[base]; if(list[level].Pos[num-4]>100.0) list[level].Pos[num-4] = 0.0; break;
		}
	}
	else {
		switch(num){
			case 0: list[level].R -= gap[base]; if(list[level].R<0.0) list[level].R = 0.0; break;
			case 1: 
			case 2:
			case 3:	list[level].Rot[num-1] -= gap[base]; if(list[level].Rot[num-1]<-360.0) list[level].Rot[num-1] = 0.0; break;
			case 4:
			case 5: 
			case 6: list[level].Pos[num-4] -= gap[base]; if(list[level].Pos[num-4]<-100.0) list[level].Pos[num-4] = 0.0; break;
		}
	}	
	/*gotoxy(0, 30);
	printf("%2d %2d %2d %6.2f %6.2f", len, level, base, list[level].R, gap[base]);*/
	ShowFlag = SHOW;
	Sleep(2);
	SendFlag = true;	
	ShowFlag = MOVE;
}

void* Show(void* data){
	while(1){
		if(ShowFlag!=0){
			cursor(0, 10);
			switch(ShowFlag){
				case SHOW: {	
							gotoxy(0, level+1);
							setcolor(WHITE); printf("%-20s", list[level].name);
							setcolor(GREEN); printf("%6.2f\t(%6.2f, %6.2f, %6.2f)\t(%6.2f, %6.2f, %6.2f)\t", list[level].R, list[level].Rot[0], list[level].Rot[1], list[level].Rot[2], list[level].Pos[0], list[level].Pos[1], list[level].Pos[2]);
							setcolor(RED);   printf("XXXDelete");
						}break; //Show Object Parameter
				case SEND: gotoxy(0, 23+i); setcolor(BLUEGREEN); printf("%5d", j); break; //Send
				case SHOWADD: {
							gotoxy(0, len+2); setcolor(WHITE*16+BLUE); printf("+ADD:");
							setcolor(WHITE); printf("%100s\n%100s", "", ""); 
							wait = false;	
						}break; // ShowAdd
				case MOVE: {
							int x, y = level+1, temp = base;
							if(temp>1) temp++;
							if(num<1) x = 25-temp;
							else if(num<4) x = 38+(num-1)*8-temp;
							else x = 62+(num-3)*8-temp;	
							gotoxy(x, y);
						}break; //Move
				case WARNTOO: {
							gotoxy(5, len+2); setcolor(RED); printf("Too many file!!! Push Any Button");
							getch();  //Warning: Too Many File
							gotoxy(5, len+2); printf("%50s", "");
							wait = false;
						}break;
				case WARNNO: {
							gotoxy(5, len+2); setcolor(RED); printf("No this file!!! Push Any Button");
							getch(); //Warning: No Such File
							gotoxy(5, len+2); printf("%50s", "");
							wait = false;
						}break;
						//Sight Parameter
				case SIGHT: gotoxy(0, 21); setcolor(GREEN); printf("%6.2f / %6.2f / (%6.2f, %6.2f, %6.2f)", eye.M, eye.E, eye.Rot[0], eye.Rot[1], eye.Rot[2]); break;
				case BUSY: gotoxy(0, 19); setcolor(RED); printf("Busy");
				case NONBUSY: gotoxy(0, 19); printf("    ");
			}
			ShowFlag = 0;	
			cursor(1, 20);
		}
	}
}

void Send(int len){	
	char str[500];

	float loc;

 	RS232_SendByte(cport_nr, '2');
	//printf("Send Clear: ");
	Read(1, "2", 0);
 	for(i=0;i<=len;i++){
		RS232_SendByte(cport_nr, '1');
		//printf("\nSend Parameter: ");		
		Read(1, "1", 0);
		sprintf(str, "%6.2f/%6.2f,%6.2f,%6.2f/%6.2f,%6.2f,%6.2f/%6.2f,%6.2f", list[i].R, list[i].Rot[0], list[i].Rot[1], list[i].Rot[2], list[i].Pos[0], list[i].Pos[1], list[i].Pos[2], list[i].max, list[i].min);
		//printf("%s, %d\n", str, strlen(str));
		RS232_SendBuf(cport_nr, str, strlen(str));
		Read(1, "b", 0);
		//printf("\nSend File: ");
		
		RS232_SendByte(cport_nr, '3');
		Read(1, "3", 0);
		for(j=0;j<list[i].length;){
			for(k=0;k<36 && j<list[i].length;k++,j++){
				RS232_SendByte(cport_nr, list[i].F[j]>>8);
				RS232_SendByte(cport_nr, list[i].F[j]);
				//usleep(500);
			}                  
			if(k==36)Read(1, "f", 0);
		}
		RS232_SendByte(cport_nr, 255);
		RS232_SendByte(cport_nr, 255);
		Read(1, "e", 0);
		ShowFlag = SEND;
	}
	RS232_SendByte(cport_nr, '4');
	Read(1, "4", 0);
}

void UpView(Sight sight){
	char str[60];
	RS232_SendByte(cport_nr, '0');
	Read(1, "0", 0);
	sprintf(str, "%6.2f/%6.2f/%6.2f,%6.2f,%6.2f", sight.M, sight.E, sight.Rot[0], sight.Rot[1],sight.Rot[2]);
	RS232_SendBuf(cport_nr, str, 34);
	Read(1, "a", 0);
}

void Read(int len, char *ch, int n){
	int d;
	char send[70];
	
	while(1){
		d = 0;
		d = RS232_PollComport(cport_nr, send, len);
		send[d] = '\0';
		if(n==0){
			if(strcmp(send, ch)==0){
				//printf("%s", send);
				break;	
			}
			else Sleep(1);
		}
		else {
			//printf("%d ", d);
			if(d==len){
				//printf("%s\n", send);
				break;	
			}
			else Sleep(10);
		} 
		//Sleep(100); 
	}
	//Sleep(1); 
}

void* child(void* data) {
	while(1){
		if(UpSightFlag){
			UpSightFlag = false;
			SendFlag = false;
			ShowFlag = BUSY;
			UpView(eye);
			Send(len);
			ShowFlag = NONBUSY;
		}
		else if(SendFlag){
			SendFlag= false;
			ShowFlag = BUSY;
			Send(len);
			ShowFlag = NONBUSY;
		}
		Sleep(100);
	}
}

void Frame(){
	setcolor(WHITE);
	gotoxy(0, 0);
	printf("Name\t\t      Rate\t\t  Rotate\t\t\tPosition");
	setcolor(WHITE*16+WHITE);
	gotoxy(0, 20);
	printf("%200s\n", "");
}


