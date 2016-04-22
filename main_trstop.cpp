#include <cmath>
#include <sstream>
#include <iostream>
#include <fstream>
#include <istream>
//#include <stdio.h>
#include <algorithm>
#include <windows.h>
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include "bird.h"
#include "DataWriter.h"
#include "MouseInput.h"
#include "BirdInput.h"
#include "Object2D.h"
#include "Sound.h"
#include "Path2D.h"
#include "Staircase.h"
#include "Circle.h"
#include "HandCursor.h"
//#include "Region2D.h"
#include "PhotoSensor.h"

#include <gl/GL.h>
#include <gl/GLU.h>
/*
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "SDL.lib")
#pragma comment(lib, "SDLmain.lib")
#pragma comment(lib, "SDL_mixer.lib")
#pragma comment(lib, "SDL_ttf.lib")
#pragma comment(lib, "SDL_image.lib")
#pragma comment(lib, "Bird.lib")
*/
#pragma push(1)


/* 
	Trajectory planning paradigm
	   -Modified from Timed Response version trtraj by AW

	   -System calibration last performed 9/18/2012 by AW

	   This experiment is modeled after Pearce and Moran 2012: http://www.sciencemag.org/content/337/6097/984.abstract
	   Here, there are two barriers, one for the start target and one for the final target.  

	   In the RT version of the task, the barrier orientations are known, but not their location (which dictates 
	      difficulty of the trajectory: depending on how where the reach target appears, the orientation and reach 
		  trajectory are dictated.  To make this variable enough to prevent pre-planning to all possibilities, 
		  we'll use 8 possible locations.
			 To generate the barriers, we can use the path-drawing functions because they have on-path detection,
		  so it will be easy to determine when subjects break the barrier instead of going around it.
		In the TR version of the task, the location (or orientation) of the barrier can change, and you lose points
		  for crashing through the barrier.  
		
*/


//state machine
enum GameState
{
	Idle = 0x01,		//00001
	Starting = 0x03,	//00011
	TRBeep = 0x04,		//00100
	Active = 0x06,		//00110
	ShowResult = 0x0A,	//01010
	Ending = 0x0C,		//01100
	Finished = 0x10		//10000
};



SDL_Event event;
SDL_Surface* screen = NULL;
/* COM ports where the Flock of Birds are connected. The first element is always
 * 0, and subsequent elements are the port numbers in order.
 */
WORD COM_port[5] = {0, 5,6,7,8};  //these need to match the ports that Windows has detected (see notes, AW for how to set up)
InputDevice* controller[BIRDCOUNT + 1];
Circle* cursors[BIRDCOUNT + 1];
HandCursor* curs[BIRDCOUNT + 1];
HandCursor* player = NULL;
Circle* targCircle;
Circle* startCircle;
Circle* photosensorCircle;
//Object2D* targettraces[NTRACES];
Image* text = NULL;
Image* text1 = NULL;
Image* text2 = NULL;
Image* text3 = NULL;
Image* text4 = NULL;
Image* text5 = NULL;
Image* text5pts = NULL;
Image* text3pts = NULL;
Image* text2pts = NULL;
Image* text1pts = NULL;
Image* text0pts = NULL;
Image* textn1pts = NULL;
Image* textn2pts = NULL;
Image* textn3pts = NULL;
Image* textn5pts = NULL;
Image* trialnum = NULL;
Sound* correctbeep = NULL;
Sound* scorebeep = NULL;
Sound* startbeep = NULL;
Sound* errorbeep = NULL;
Sound* stopbeep = NULL;
TTF_Font* font = NULL;
TTF_Font* trialnumfont = NULL;
SDL_Color textColor = {0, 0, 0};
GLfloat GoodPathClr[3] = {.4f, .4f, .4f};
GLfloat HitPathClr[3] = {1.0f, 0.5f, 0.0f};
DataWriter* writer = NULL;
GameState state;
Uint32 gameTimer;
Uint32 hoverTimer;
Uint32 TstartTrial = 0;
bool badlat = false;
bool targetHit = false;
Uint32 targetTime;
bool birds_connected;
Path2D spath[NPATHS+1];  //barriers at each of the possible orientations around the start target
Path2D tpath[NPATHS+1][4];  //barriers at each of the possible orientations around the reach target

//variables to show hand path (KR)
float trialData[10000][2];
int dataIndex = 0;
int dataEndIndex = 0;

//staircase-tracking variables
Staircase Stairs[8][8];  //index into this by the rotated jump/target condition. Since we are only jumping the start barrier, ind0 = original start barrier, ind1 = final start barrier (normalized orientations)

//FTDI photosensor variables
FT_HANDLE ftHandle;
bool sensorsActive;


BIRDSYSTEMCONFIG sysconfig;

//bool updatedisplay = true;

//variables to compute the earned score
int score = 0;

/*
//constants for Timed Response timing:
int INTER_TONE_INTERVAL = 500;
int MAX_TONE_RINGS = 4;
int currToneRingNum = 0;
int lastToneRing = 0;
Uint32 TJumpTime = 0;
*/

// Trial table structure, to keep track of all the parameters for each trial of the experiment
typedef struct {
	int TrialType;			// Flag for type of trial: 1 = TR task (jump barrier orientation or target position)
	float startx,starty;	// x/y pos of start target; also, trace will be centered here!
	float xpos,ypos;		// x/y pos of the original target.
	int spath;				//start target barrier to display (centered on start target - use pathx and pathy)
	int tpath;				//reach target barrier to display (centered on reach target - use pathx and pathy)
	int stoptime;			//time that the stop signal occurs  (set to zero value for no stop signal, or negative values to use the staircase files to determine the exact time)
	//int trace;				//trace (>= 0) for cuing how to draw the path, or -1 otherwise.
	int tgt;				//number code for where the target is located
	int iti;				//waiting time between trials (counted from the start of the STARTING state, e.g. time before trial onset)
} TRTBL;

#define TRTBL_SIZE 1000
TRTBL trtbl [TRTBL_SIZE];


int NTRIALS = 0;
int CurTrial = 0;

#define curtr trtbl[CurTrial]

//target structure; keep track of where the current target is now (only 1!)
TargetFrame Target;

float targetColor[3] = {0.0f, 0.0f, 1.0f};
float cursColor[3] = {0.13f, .55f, .13f};
float startColor[3] = {.0f,1.0f,1.0f};
float blkColor[3] = {0.0f, 0.0f, 0.0f};
float whiteColor[3] = {1.0f, 1.0f, 1.0f};
float earlyColor[3] = {0.0f, 1.0f, 0.0f};
float lateColor[3] = {0.0f, 0.0f, 0.0f};
float stopColor[3] = {1.0f, 0.0f, 0.0f};
float ontimeColor[3] = {1.0f, 1.0f, 1.0f};


//structure to keep track of what to draw in the draw_screen() function
typedef struct {
	int drawbird[BIRDCOUNT + 1];  //array of which cursors to draw
	//int drawtrace;				  //trace number to draw (for intro screen)
	int drawspath;                //path number to draw
	int drawtpath[5];                //path number to draw
	bool drawhandpath;			  //draw saved hand path (KR)
	//int drawstart;               //draw the start target?
	//bool drawtgt;                 //draw the reach target?
	int drawtext[11];                //write feedback text, depending on what flags are set
} DRAWSTRUC;

DRAWSTRUC drawstruc;

// Initializes everything and returns true if there were no errors
bool init();
// Initializes the Flock of Birds and returns true if successful
bool init_fob();
// Sets up OpenGL
void setup_opengl();
// Performs closing operations
void clean_up();
// Draws objects on the screen
void draw_screen();

//file to load in trial table
int LoadTrFile(char *filename);

//file to load in staircase table
int LoadStairTbl();
int WriteStairTbl();


// Update loop (state machine)
void game_update();


int main(int argc, char* args[])
{
	int a = 0;

	std::cerr << "Start main." << std::endl;


	SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
	//HIGH_PRIORITY_CLASS
	std::cerr << "Promote process priority to Above Normal." << std::endl;

	if (!init())
	{
		// There was an error during initialization
		std::cerr << "Initialization error." << std::endl;
		return 1;
	}
	
	bool quit = false;
	while (!quit)
	{
		//game_update(); // Run the game loop

		bool inputs_updated = false;

		// Retrieve Flock of Birds data
		if (birds_connected)
		{
			// Update inputs from Flock of Birds
			inputs_updated = BirdInput::ProcessData();
		}

		// Handle SDL events
		while (SDL_PollEvent(&event))
		{
			// See http://www.libsdl.org/docs/html/sdlevent.html for list of event types
			if (event.type == SDL_MOUSEMOTION)
			{
				inputs_updated |= !birds_connected; // Record data if birds are not connected
				MouseInput::ProcessEvent(event); // Pass this event to the MouseInput class to process
			}
			else if (event.type == SDL_KEYDOWN)
			{
				// See http://www.libsdl.org/docs/html/sdlkey.html for Keysym definitions
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					quit = true;
				}

			}
			else if (event.type == SDL_QUIT)
			{
				quit = true;
			}
		}

		if ((CurTrial >= NTRIALS) && (state == Finished) && (SDL_GetTicks() - gameTimer >= 10000))
			quit = true;

		// Get data from input devices
		if (inputs_updated) // if there is a new frame of data
		{
			
			if (sensorsActive)
			{
				UCHAR bit = 4;  //this is the CTS line
				Target.PSstatus = PhotoSensor::GetSensorBitBang(ftHandle,bit);
			}
			else
				Target.PSstatus = -99;


			//update the cursor position
			//updatedisplay = true;
			for (a = (birds_connected ? 1 : 0); a <= (birds_connected ? BIRDCOUNT : 0); a++)
			{
				InputFrame i = controller[a]->GetFrame();

				curs[a]->UpdatePos(i.x,i.y);
				i.vel = curs[a]->GetVel();  //write out the velocity of the ACTIVE bird to ALL channels!
				writer->Record(a, i, Target);
				
			}
			
			//update the data array
			if (birds_connected)  //birds connected; use Right hand input
			{
				trialData[dataIndex][0] = curs[HAND]->GetX();
				trialData[dataIndex][1] = curs[HAND]->GetY();
			}
			else //birds not connected; use mouse input
			{
				trialData[dataIndex][0] = curs[0]->GetX();
				trialData[dataIndex][1] = curs[0]->GetY();
			}
			dataIndex = (++dataIndex >= 9999 ? 9999 : dataIndex);   //if we exceed the array, just keep replacing the last data point as "junk"
			
		}

		game_update(); // Run the game loop (state machine update)

		//if (dodraw)  //reduce number of calls to draw_screen -- does this speed up display/update?
			draw_screen();

		//SDL_Delay(1);

	}

	clean_up();

	return 0;
}


//function to read in the name of the trial table file, and then load that trial table
int LoadTrFile(char *fname)
{

	//std::cerr << "LoadTrFile begin." << std::endl;

	char tmpline[100] = ""; 
	int ntrials = 0;

	
	//read in the trial file name
	std::ifstream trfile(fname);

	if (!trfile)
	{
		std::cerr << "Cannot open input file." << std::endl;
		return -1;
		//exit(1);
	}
	else
		std::cerr << "Opened TrialFile " << TRIALFILE << std::endl;

	trfile.getline(tmpline,sizeof(tmpline),'\n');
	
	while(!trfile.eof())
	{
		sscanf(tmpline, "%d %f %f %f %f %d %d %d %d %d %d", &trtbl[ntrials].TrialType, &trtbl[ntrials].startx,&trtbl[ntrials].starty,
			&trtbl[ntrials].xpos,&trtbl[ntrials].ypos,
			&trtbl[ntrials].spath,
			&trtbl[ntrials].tpath,
			&trtbl[ntrials].stoptime,
			&trtbl[ntrials].tgt,
			&trtbl[ntrials].iti);

			ntrials++;
			trfile.getline(tmpline,sizeof(tmpline),'\n');
	}


	trfile.close();
	if(ntrials == 0)
	{
		std::cerr << "Empty input file." << std::endl;
		//exit(1);
		return -1;
	}
	std::cerr << ntrials << " trials found. " << std::endl;
	return ntrials;
}


int LoadStairTbl()
{

	char tmpline[100] = ""; 
	int a,b,c,temp1, temp2, temp3;
	FILE *fid;
	
	for (a = 0; a < 8; a++)
	{
		for (b = 0; b < 8; b++)
		{
			tmpline[0] = '\0';
			std::sprintf(tmpline,"Staircase/stair_%d_%d.txt",a,b);

			fid = fopen(tmpline,"r");

			if (fid == NULL)
			{
				std::cerr << "Cannot open Staircase file " << tmpline << std::endl;
				continue;
			}

			fscanf(fid,"%d %d",&temp1,&temp2);
			Stairs[a][b].SetMean(temp1);
			Stairs[a][b].SetRange(temp2);
			Stairs[a][b].SetnPoints(STAIRPOINTCOUNT);
			Stairs[a][b].SetPoints();

			
			while (fscanf(fid,"%d %d %d",&temp1,&temp2,&temp3) != EOF)
			{
				Stairs[a][b].AddDataPoint(temp1,temp2,temp3);
			}

			fclose(fid);

			

		}
	}


	return 0;

}


int WriteStairTbl()
{

	char tmpline[100] = ""; 
	int a,b,c, temp1, temp2, temp3;
	FILE *fid;

	for (a = 0; a < 8; a++)
	{
		for (b = 0; b < 8; b++)
		{

			//don't re-save data if there are no trials of that type (that way we can look for just the updated files
			if (Stairs[a][b].GetiTrial() <= 0)
				continue;

			tmpline[0] = '\0';
			std::sprintf(tmpline,"Staircase/stair_%d_%d.txt",a,b);

			fid = fopen(tmpline,"w");

			if (fid == NULL)
			{
				std::cerr << "Cannot open Staircase file " << tmpline << std::endl;
				continue;
			}

			fprintf(fid,"%d %d\n",Stairs[a][b].GetMean(),Stairs[a][b].GetRange());

			
			c = 0;
			while (c < Stairs[a][b].GetiTrial())
			{

				Stairs[a][b].GetDataPoint(c,temp1,temp2,temp3);
				fprintf(fid,"%d %d %d\n",temp1,temp2,temp3);
				c++;
			}
			

			fclose(fid);

		}
	}


	return 0;

}



//initialization function - set up the experimental environment and load all relevant parameters/files
bool init()
{
	// Initialize Flock of Birds
	/* The program will run differently if the birds fail to initialize, so we
	 * store it in a bool.
	 */

	int a;
	char tmpstr[80];
	//char teststr[30];
	char fname[50] = TRIALFILE;
	int b;

	//std::cerr << "Start init." << std::endl;

	birds_connected = init_fob();
	//birds_connected = false; //override this for now, for testing purposes.
	if (!birds_connected)
		std::cerr << "No birds initialized. Mouse mode." << std::endl;

	// Initialize SDL, OpenGL, SDL_mixer, and SDL_ttf
	if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
	{
		std::cerr << "SDL failed to intialize."  << std::endl;
		return false;
	}
	else
		std::cerr << "SDL initialized." << std::endl;

	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP,
		SDL_OPENGL | (WINDOWED ? 0 : SDL_FULLSCREEN));
	if (screen == NULL)
	{
		std::cerr << "Screen failed to build." << std::endl;
		return false;
	}
	else
		std::cerr << "Screen built." << std::endl;
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0); //disable vsync

	setup_opengl();
	//Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 8, 4096);
	Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 512);  //open a smaller buffer, which addresses timing issues (DMH)
	if (TTF_Init() == -1)
	{
		std::cerr << "Audio failed to initialize." << std::endl;
		return false;
	}
	else
		std::cerr << "Audio initialized." << std::endl;

	//turn off the computer cursor
	SDL_ShowCursor(0);


	// Load files and initialize pointers
	font = TTF_OpenFont("Resources/arial.ttf", 28);
	
	/*
	Image* tgttraces[NTRACES+1];  //is there a limit to the size of this array (stack limit?).  cannot seem to load more than 10 image traces...

	//load all the trace files; traces start at 0!
	for (a = 0; a < NTRACES; a++)
	{
		sprintf(tmpstr,"%s/Trace%d.png",TRACEPATH,a);
		tgttraces[a] = Image::LoadFromFile(tmpstr);
		if (tgttraces[a] == NULL)
			std::cerr << "Image Trace" << a << " did not load." << std::endl;
		else
		{
			targettraces[a] = new Object2D(tgttraces[a]);
			std::cerr << "   Trace " << a << " loaded." << std::endl;
		}
	}
	*/
	
	std::cerr << "Images loaded." << std::endl;

	//set up target circles
	startCircle = new Circle(curtr.startx, curtr.starty, START_RADIUS*2, startColor);
	startCircle->setBorderWidth(0.001f);
	startCircle->On();
	startCircle->BorderOn();

	targCircle = new Circle(curtr.xpos, curtr.ypos, TARGET_RADIUS*2, targetColor);
	targCircle->BorderOn();
	targCircle->Off();

	photosensorCircle = new Circle(0.0f,0.23f,0.04,blkColor);
	photosensorCircle->SetBorderColor(blkColor);
	photosensorCircle->BorderOff();
	photosensorCircle->On();

	//load the path files
	for (a = 0; a < NPATHS; a++)
	{
		sprintf(tmpstr,"%s/Path%d.txt",PATHPATH,a); 
		spath[a] = Path2D::LoadPathFromFile(tmpstr);
		tpath[a][0] = Path2D::LoadPathFromFile(tmpstr);
		tpath[a][1] = Path2D::LoadPathFromFile(tmpstr);
		tpath[a][2] = Path2D::LoadPathFromFile(tmpstr);
		tpath[a][3] = Path2D::LoadPathFromFile(tmpstr);
		if (spath[a].GetPathNVerts() < 0)
			std::cerr << "   Path " << a << " did not load." << std::endl;
		else
			std::cerr << "   Path " << a << " loaded." << std::endl;
	}


	//load trial table from file
	NTRIALS = LoadTrFile(fname);

	if(NTRIALS == -1)
	{
		std::cerr << "Trial File did not load." << std::endl;
		return false;
	}
	else
		std::cerr << "Trial File loaded: " << NTRIALS << " trials found." << std::endl;

	//assign the data-output file name based on the trial-table name 
	std::string savfile;
	savfile.assign(fname);
	savfile.insert(savfile.rfind("."),"_data");

	std::strcpy(fname,savfile.c_str());

	std::cerr << "SavFileName: " << fname << std::endl;

	writer = new DataWriter(&sysconfig,fname);  //set up the data-output file

	//load in the current staircase table
	LoadStairTbl();

	Stairs;


	//initialize the photosensor
	int status = -5;
	int devNum = 0;

	FT_STATUS ftStatus; 
	DWORD numDevs;

	UCHAR Mask = 0x0f;  
	//the bits in the upper nibble should be set to 1 to be output lines and 0 to be input lines (only used 
	//  in SetSensorBitBang() ). The bits in the lower nibble should be set to 1 initially to be active lines.

	status = PhotoSensor::InitSensor(devNum,&ftHandle,1,Mask);
	std::cerr << "PhotoSensor: " << status << std::endl;

	//status = PhotoSensor::SetSensorBitBang(ftHandle,Mask,3,0);

	UCHAR dataBit;

	//status = PhotoSensor::SetSensorBitBang(ftHandle,Mask,2,1);
	FT_GetBitMode(ftHandle, &dataBit);
	
	std::cerr << "DataByte: " << dataBit << std::endl;
	
	if (status==0)
	{
		printf("PhotoSensor found and opened.\n");
		sensorsActive = true;
	}
	else
	{
		if (status == 1)
			std::cerr << "   Failed to create device list." << std::endl;
		else if (status == 2)
			std::cerr << "   Sensor ID=" << devNum << " not found." << std::endl;
		else if (status == 3)
			std::cerr << "   Sensor " << devNum << " failed to open." << std::endl;
		else if (status == 4)
			std::cerr << "   Sensor " << devNum << " failed to start in BitBang mode." << std::endl;
		else
			std::cerr << "UNDEFINED ERROR!" << std::endl;

		sensorsActive = false;
	}
	


	// Assign array index 0 of controller and cursor to correspond to mouse control
	controller[0] = new MouseInput();

	if (birds_connected)
	{
		/* Assign birds to the same indices of controller and cursor that they use
		 * for the Flock of Birds
		 */

		for (a = 1; a <= BIRDCOUNT; a++)
		{
			controller[a] = new BirdInput(a);
			cursors[a] = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*2, cursColor);
			cursors[a]->BorderOff();
			curs[a] = new HandCursor(cursors[a]); 
			curs[a]->SetOrigin(curtr.startx, curtr.starty);
		}
		// Use bird 3: the right hand
		player = curs[HAND];  //right hand is bird #3
	}
	else
	{
		// Use mouse control
		cursors[0] = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*2, cursColor);
		curs[0] = new HandCursor(cursors[0]);
		curs[0]->SetOrigin(curtr.startx, curtr.starty);
		player = curs[0];

		sysconfig.dMeasurementRate = 0;
	}	


	//load sound files
	correctbeep = new Sound("Resources/correctbeep4.wav");
	scorebeep = new Sound("Resources/correctbeep.wav");
	startbeep = new Sound("Resources/startbeep.wav");
	errorbeep = new Sound("Resources/errorbeep2.wav");
	stopbeep = new Sound("Resources/errorbeep1.wav");

	/* To create text, call a render function from SDL_ttf and use it to create
	 * an Image object. See http://www.libsdl.org/projects/SDL_ttf/docs/SDL_ttf.html#SEC42
	 * for a list of render functions.
	 */
	text = new Image(TTF_RenderText_Blended(font, " ", textColor));

	std::stringstream textstring;		
	textstring.str("");
	textstring << "You came too close to the start barrier!";
	text1 = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "You came too close to the target barrier!";
	text2 = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "You MUST begin your movement ON the Fourth Tone!";
	text3 = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "You failed to cancel your movement!";
	text4 = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "You missed the target!";
	text5 = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "5 points!";
	text5pts = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "3 points.";
	text3pts = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "2 points.";
	text2pts = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "1 points.";
	text1pts = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "0 points.";
	text0pts = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "-1 points.";
	textn1pts = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "-2 points.";
	textn2pts = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "-3 points.";
	textn3pts = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "-5 points!";
	textn5pts = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));


	trialnumfont = TTF_OpenFont("Resources/arial.ttf", 12);
	trialnum = new Image(TTF_RenderText_Blended(trialnumfont, "1", textColor));


	SDL_WM_SetCaption("TR_Traj_task", NULL);

	
	// Set the initial game state
	state = Idle; 


	std::cerr << "initialization complete." << std::endl;
	return true;
}

bool init_fob()
{

	int temp;

		/*	//set up system configuration
			birdGetSystemConfig(GROUP_ID,&sysconfig); //get defaults
			sysconfig.dMeasurementRate = SAMPRATE;  //edit sampling rate
			temp = birdSetSystemConfig(GROUP_ID,&sysconfig); //set sampling rate
			std::cerr << "sys config set: " << temp << std::endl;
			std::cerr << birdGetErrorMessage() << std::endl;
			birdGetSystemConfig(GROUP_ID,&sysconfig); //re-update saved values
			*/

	if (birdRS232WakeUp(GROUP_ID, FALSE, BIRDCOUNT, COM_port, BAUD_RATE,
		READ_TIMEOUT, WRITE_TIMEOUT, GMS_GROUP_MODE_NEVER))
	{
		
		//set up system configuration - must do this BEFORE starting to stream
		birdGetSystemConfig(GROUP_ID,&sysconfig); //get defaults
		sysconfig.dMeasurementRate = SAMPRATE;  //edit sampling rate
		temp = birdSetSystemConfig(GROUP_ID,&sysconfig); //set sampling rate
		std::cerr << "FOB Sys config set: " << temp << std::endl;
		//std::cerr << birdGetErrorMessage() << std::endl;
		birdGetSystemConfig(GROUP_ID,&sysconfig); //re-update saved values

		if (birdStartFrameStream(GROUP_ID))
		{
			return true;
		}
		else
		{
			birdShutDown(GROUP_ID);
		}
	}
	// There was an error
	return false;
}

float lineWidth[2];

static void setup_opengl()
{
	glClearColor(255, 255, 255, 0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	/* The default coordinate system has (0, 0) at the bottom left. Width and
	 * height are in meters, defined by PHYSICAL_WIDTH and PHYSICAL_HEIGHT
	 * (config.h). If MIRRORED (config.h) is set to true, everything is flipped
	 * horizontally.
	 */
	glOrtho(MIRRORED ? PHYSICAL_WIDTH : 0, MIRRORED ? 0 : PHYSICAL_WIDTH,
		0, PHYSICAL_HEIGHT, -1.0f, 1.0f);

	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_POLYGON_SMOOTH);

	glGetFloatv(GL_LINE_WIDTH_RANGE, lineWidth);
	std::cerr << "MaxLineWidth " << lineWidth[1] << std::endl;
}


//end the program; clean up everything neatly.
void clean_up()
{

	WriteStairTbl();

	for (int a = 0; a <= (birds_connected ? BIRDCOUNT : 0); a++)
	{
		delete controller[a];
	}
	delete startCircle;
	delete targCircle;
	delete photosensorCircle;
	delete scorebeep;
	delete startbeep;
	delete errorbeep;
	delete correctbeep;
	
	/*
	for (int a = 0; a < NTRACES; a++)
		delete targettraces[a];
		*/

	delete text;
	delete text1;
	delete text2;
	delete text3;
	//delete text4;
	delete text5;
	delete text5pts;
	delete text3pts;
	delete text2pts;
	delete text1pts;
	delete text0pts;
	delete textn1pts;
	delete textn2pts;
	delete textn3pts;
	delete textn5pts;
	delete trialnum;
	
	delete writer;
	Mix_CloseAudio();
	TTF_CloseFont(font);
	TTF_Quit();
	SDL_Quit();
	if (birds_connected)
	{
		birdStopFrameStream(GROUP_ID);
		birdShutDown(GROUP_ID);
	}
}

//control what is drawn to the screen
static void draw_screen()
{
	int a;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	/*
	//draw the trace specified
	if (drawstruc.drawtrace >= 0)
	{
		targettraces[drawstruc.drawtrace]->SetPos(curtr.startx,curtr.starty);
		targettraces[drawstruc.drawtrace]->Draw();
		Target.trace = drawstruc.drawtrace;
	}
	else
		Target.trace = -1;
		*/
	
		
	//draw the path
	if (drawstruc.drawspath >= 0)
	{
		spath[drawstruc.drawspath].Draw(curtr.startx,curtr.starty);
		Target.spath = drawstruc.drawspath;
	}
	else
		Target.spath = -100;

	if (drawstruc.drawtpath[0] >= 0)
	{
		tpath[drawstruc.drawtpath[0]][0].Draw(targCircle->GetX(),targCircle->GetY());
		Target.tpath = drawstruc.drawtpath[0];
	}
	if (drawstruc.drawtpath[1] >= 0)
	{
		tpath[drawstruc.drawtpath[1]][0].Draw(TGTPOS1X,TGTPOS1Y);
		tpath[drawstruc.drawtpath[1]][1].Draw(TGTPOS2X,TGTPOS2Y);
		tpath[drawstruc.drawtpath[1]][2].Draw(TGTPOS3X,TGTPOS3Y);
		tpath[drawstruc.drawtpath[1]][3].Draw(TGTPOS4X,TGTPOS4Y);
		if (drawstruc.drawtpath[0] < 0)
            Target.tpath = 0-drawstruc.drawtpath[1];  //save target path if tpath[1] is on at all locations, but the true tpath is not on at the correct location (tpath[0])
	}
	else if (drawstruc.drawtpath[0] < 0 && drawstruc.drawtpath[0] < 0)
		Target.tpath = -100;


	
	//draw the hand path, if requested
	if (drawstruc.drawhandpath)
	{
		// Draw path
		glDisable(GL_TEXTURE_2D);
		glColor3f(0.13f,0.55f,0.13f);
		glLineWidth((CURSOR_RADIUS*2)/PHYSICAL_RATIO);
		//glLineWidth(6.0f);
		//glLineWidth(min((CURSOR_RADIUS*2)/PHYSICAL_RATIO,lineWidth[1]));

		glBegin(GL_LINE_STRIP);

		for (int a = 0; a<=dataEndIndex; a++)
		{
			glVertex3f(trialData[a][0],trialData[a][1],0.0f);
		}

		glEnd();

		//reset defaults after the draw
		glColor3f(1.0f,1.0f,1.0f);
		glLineWidth(1.0f);

	}
	

/*
	// Draw the start marker, if true
	if (drawstruc.drawstart == -1)  //draw feedback start marker: early
	{
		startCircle->SetColor(earlyColor);
	}
	else if (drawstruc.drawstart == 0) //draw feedback start marker: on time
	{
		startCircle->SetColor(whiteColor);
	}
	else if (drawstruc.drawstart == 1) //draw feedback start marker: late
	{
		startCircle->SetColor(lateColor);
	}
	else
	{
		startCircle->SetColor(startColor);
	}
	*/

	startCircle->Draw();
	if (startCircle->drawState())  //draw default start marker
	{
		//startCircle->On();
		Target.startx = startCircle->GetX();
		Target.starty = startCircle->GetY();
	}
	else
	{
		Target.startx = -100;
		Target.starty = -100;
	}


	// Draw the target marker for the current trial, if true
	targCircle->Draw();
	if (targCircle->drawState())
	{
		// Marker is stretched to the activation radius
		Target.tgtx = targCircle->GetX();
		Target.tgty = targCircle->GetY();
	}
	else
	{
		Target.tgtx = -100;
		Target.tgty = -100;
	}

	photosensorCircle->Draw();
	
	//draw only the mouse/birds requested, as initialized in init()
	player->Draw();

	// Draw text - provide feedback
	if (drawstruc.drawtext[0] == 1)
	{
		//provide the score at the end of the block.
		std::stringstream scorestring;
			scorestring << "You earned " 
				        << score 
						<< " points.";
			text = new Image(TTF_RenderText_Blended(font, scorestring.str().c_str(), textColor));
			text->Draw(0.6f, 0.47f);
			for (a=1; a<10; a++)
				drawstruc.drawtext[a] = 0;
			drawstruc.drawtext[4] = -99;
	}

	if (drawstruc.drawtext[1] == 1)
		text1->Draw(0.6f,0.57f);
	if (drawstruc.drawtext[2] == 1)
		text2->Draw(0.6f,0.545f);
	if (drawstruc.drawtext[3] == 1)
		text3->Draw(0.6f,0.52f);
	if (drawstruc.drawtext[4] == 1)
		text4->Draw(0.6f,0.52f);
	if (drawstruc.drawtext[5] == 1)
		text5->Draw(0.6f,0.495f);
	if (drawstruc.drawtext[10] > -90)
	{
		
		if (drawstruc.drawtext[10] == 5)
			text5pts->Draw(0.6f, 0.45f);
		else if (drawstruc.drawtext[10] == 3)
			text3pts->Draw(0.6f, 0.45f);
		else if (drawstruc.drawtext[10] == 2)
			text2pts->Draw(0.6f, 0.45f);
		else if (drawstruc.drawtext[10] == 1)
			text1pts->Draw(0.6f, 0.45f);
		else if (drawstruc.drawtext[10] == -1)
			textn1pts->Draw(0.6f, 0.45f);
		else if (drawstruc.drawtext[10] == -2)
			textn2pts->Draw(0.6f, 0.45f);
		else if (drawstruc.drawtext[10] == -3)
			textn3pts->Draw(0.6f, 0.45f);
		else if (drawstruc.drawtext[10] == -5)
			textn5pts->Draw(0.6f, 0.45f);
		else
			text0pts->Draw(0.6f, 0.45f);

	}



	//write the trial number
	trialnum->Draw(PHYSICAL_WIDTH*23/24, PHYSICAL_HEIGHT*23/24);


	SDL_GL_SwapBuffers();
	glFlush();

	//updatedisplay = false;
}

//game update loop - state machine controlling the status of the experiment
bool handflag = false;			//flag to see if the hand started moving (for use with calculating latency)

bool intersectstartpath = false; //flag to see if the start path has been intersected
bool intersecttgtpath = false;	 //flag to see if the target path has been intersected
bool targetClose = false;		 //flag to see if subject ever approached the target at all

Uint32 handTimer;				//timer to save the onset time of the hand movement - for latency calculation
Uint32 fourthBeep;				//time that the last beep occurred
Uint32 changeBeep;				//time that the 2nd beep occured, for calculating the time when the target/barrier jump should occur

int indx;
int trscore = 0;					//score for the current trial;

int beepNum;

bool mvmtStarted = false;  //flag to determine when the hand velocity goes above threshold
bool mvmtEnded = false;    //flag to determine when the hand velocity goes below threshold
Uint32 mvmtTimer;         //timer to determine when the hand velocity fell below threshold

float handangle;
int jumpsuccess;

int rotspath, rottpath;


void game_update()
{
	int a;

	switch (state)
	{
		case Idle:
			/* If player starts hovering over start marker, set state to Starting
			 * and store the time -- this is for trial #1 only!
			 */
			//drawstruc.drawtrace = -1; //for normal operations, show no trace
			startCircle->SetPos(curtr.startx, curtr.starty);
			startCircle->SetBorderColor(blkColor);
			startCircle->SetColor(startColor);
			startCircle->On();

			photosensorCircle->On();

			drawstruc.drawspath = -1;
			drawstruc.drawtpath[0] = -1;
			drawstruc.drawtpath[1] = -1;
			targCircle->Off();

			for (a = 0; a < 10; a++)
				drawstruc.drawtext[a] = 0;
			drawstruc.drawtext[10] = -99;

			drawstruc.drawhandpath = false;
			dataIndex = 0;
			//drawstruc.drawvelbar = -1;
			Target.score = -10;
			Target.lat = -5000;
			//drawstruc.drawtrace = -1;

			Target.trial = 0;
			Target.stop = 0;

			//if cursor is in the start target, move to Starting state.
			if( (player->Distance(startCircle) <= START_RADIUS*1.5) && (CurTrial < NTRIALS) )
			{
				
				hoverTimer = SDL_GetTicks();
				gameTimer = SDL_GetTicks();
				std::cerr << "Leaving IDLE state." << std::endl;
				handflag = false;
				mvmtStarted = false;
				mvmtEnded = false;

				Target.score = -1;
				Target.lat = -5000;
				intersectstartpath = false;
				intersecttgtpath = false;

				state = Starting;
			}
			break;

		case Starting: //beginning of the trial
			/* If player stops hovering over start marker, set state to Idle and
			 * store the time.  Otherwise, go to active state
			 */
			//targettraces[curtr.trace]->SetPos(curtr.startx, curtr.starty);
			//drawstruc.drawtrace = curtr.trace;
			//drawstruc.drawtrace = -1;
			curtr.spath = curtr.spath;

			startCircle->SetPos(curtr.startx, curtr.starty);
			startCircle->SetBorderColor(blkColor);
			startCircle->SetColor(startColor);
			startCircle->On();

			drawstruc.drawspath = -1;
			drawstruc.drawtpath[0] = -1;
			drawstruc.drawtpath[1] = -1;
			for (a = 0; a < NPATHS; a++)
			{
				spath[a].SetPathColor(GoodPathClr);
				tpath[a][0].SetPathColor(GoodPathClr);
				tpath[a][1].SetPathColor(GoodPathClr);
				tpath[a][2].SetPathColor(GoodPathClr);
				tpath[a][3].SetPathColor(GoodPathClr);
			}
			//drawstruc.drawtrace = -1;
			targCircle->Off();
			drawstruc.drawtext[10] = -99;

			drawstruc.drawhandpath = false;
			dataIndex = 0;
			
			Target.score = -10;
			trscore = 0;
			Target.lat = -5000;
			targetClose = false;

			
			if ((player->Distance(startCircle) > START_RADIUS*1.5) || (player->GetVel() >= VEL_MVT_TH))
			{
				state = Idle;
			}
			// If player hovers long enough, set state to Active
			else if (SDL_GetTicks() - hoverTimer >= curtr.iti)
			{
				
				Target.trial = -(CurTrial+1);

				if (curtr.stoptime < 0)
				{
					rotspath = curtr.spath-curtr.tgt;
					rotspath = (rotspath>=0 ? rotspath : rotspath+8);

					rottpath = curtr.tpath-curtr.tgt;
					rottpath = (rottpath>=0 ? rottpath : rottpath+8);

					//Stairs[rotspath][rottpath].AddPointCount(-curtr.stoptime-1);
					curtr.stoptime = int(2*IBI)-Stairs[rotspath][rottpath].GetPoint(-curtr.stoptime-1);

				}

				Target.stoptime = curtr.stoptime;
				
				std::stringstream texttn;
				texttn << CurTrial+1;  //CurTrial starts from 0, so we add 1 for convention.
				delete trialnum;
				trialnum = new Image(TTF_RenderText_Blended(trialnumfont, texttn.str().c_str(), textColor));
				std::cerr << "Trial " << CurTrial+1 << " started at " << SDL_GetTicks() << "." << std::endl;
				
				targCircle->SetPos(curtr.xpos, curtr.ypos);
				targCircle->On();
				
				drawstruc.drawspath = curtr.spath;
				drawstruc.drawtpath[0] = curtr.tpath;
				//drawstruc.drawtrace = curtr.trace;
				
				handflag = false;
				badlat = false;
				mvmtStarted = false;
				mvmtEnded = false;

				TstartTrial = SDL_GetTicks();

				intersectstartpath = false;
				intersecttgtpath = false;

				photosensorCircle->Off();

				startbeep->Play();
				gameTimer = SDL_GetTicks();
				std::cerr << "Leaving STARTING state." << std::endl;
				beepNum = 0;
				state = TRBeep;
			}

			break;

		
		case TRBeep:

			startCircle->On();
			drawstruc.drawhandpath = false;

			//detect the time the hand leaves the target. this only can happen once per trial! -- use velocity or distance criterion to be sure to catch the trigger
			if ((!mvmtStarted && (player->GetVel() >= VEL_MVT_TH && (player->Distance(startCircle) > START_RADIUS*1.5))) || (!handflag && (player->Distance(startCircle) > START_RADIUS*2.5)))
			{
				handTimer = SDL_GetTicks();
				handflag = true;
				mvmtStarted = true;
				std::cerr << "Mvmt Started: " << float(SDL_GetTicks()) << std::endl;
				mvmtEnded = false;
				
			}

			//if we have started moving, turn off the display for the remainder of the state
			if (mvmtStarted) //(handflag)
			{
				startCircle->On();
				targCircle->On();
				drawstruc.drawspath = -1;
				drawstruc.drawtpath[0] = -1;
				drawstruc.drawtpath[1] = -1;
				//drawstruc.drawtrace = -1;

				//we will do the following checks if the hand has begun moving, in case any event happens before we get to the active state:

				//note if the subject intersected either path (on the path or has crossed the path)
				if (spath[curtr.spath].OnPath(player,curtr.startx,curtr.starty) || spath[curtr.spath].PathCollision(player,curtr.startx,curtr.starty) )
				{
					intersectstartpath = true;
				}

				if (tpath[curtr.tpath][0].OnPath(player,curtr.xpos,curtr.ypos) || tpath[curtr.tpath][0].PathCollision(player,curtr.xpos,curtr.ypos) )
				{
					intersecttgtpath = true;
				}

				//note if/the time when the subject entered the target
				if (player->Distance(targCircle) < TARGET_RADIUS*2){
					if(!targetHit)
					{
						targetHit = true;
						targetTime = SDL_GetTicks();
					}
				}
				else
					targetHit = false;  //if you leave the target at any time, invalidate the flag

				if (player->Distance(targCircle) < TARGET_RADIUS*3)
					targetClose = true;

				
				if (!mvmtEnded && (player->GetVel() < VEL_MVT_TH) && (handTimer-SDL_GetTicks())>200 && player->Distance(startCircle)>4*START_RADIUS )
				{
					mvmtEnded = true;
					mvmtTimer = SDL_GetTicks();
					std::cerr << "Mvmt Ended: " << float(SDL_GetTicks())  << std::endl;
				}
				if (player->GetVel() >= VEL_MVT_TH)  //update flag
				{
					mvmtEnded = false;
					mvmtTimer = SDL_GetTicks();
				}

			}

			//count time after the second beep to determine when to change target
			if(beepNum > 0 && curtr.stoptime != 0 && (SDL_GetTicks() - changeBeep) > curtr.stoptime)
			{
				startCircle->SetColor(stopColor);

				targCircle->On();
				drawstruc.drawspath = curtr.spath;
				drawstruc.drawtpath[0] = curtr.tpath;
				drawstruc.drawtpath[1] = -1;  //comment this line to keep all paths on screen even after target appears, for RTfloor experiment

				photosensorCircle->On();

				Target.stop = 1;

			}

			if (SDL_GetTicks() - gameTimer > IBI)  //if surpass the inter-beep interval time, cue the next beep
			{
				gameTimer = SDL_GetTicks();
				startbeep->Play();
				beepNum++;

				if(beepNum == 1)  //if this is the 2nd beep, note it for calculation of the target/barrier jump time
				{
					changeBeep = gameTimer;
				}
				else if(beepNum == 3)  //if this is the last beep (go cue), note it and move on to the next state
				{
					fourthBeep = gameTimer;
					std::cerr << "Leaving TRBEEP state." << std::endl;
					beepNum = 0;
					targetHit = false;
					Target.trial = CurTrial+1;

					photosensorCircle->On();

					state = Active;
				}
			}

			break;

		case Active:
			
			//targettraces[curtr.trace]->SetPos(curtr.startx, curtr.starty);
			//drawstruc.drawtrace = curtr.trace;

			//detect the onset of hand movement, for calculating latency -- use velocity or distance criterion
			if ((!mvmtStarted && (player->GetVel() >= VEL_MVT_TH && (player->Distance(startCircle) > START_RADIUS*1.5))) || (!handflag && (player->Distance(startCircle) > START_RADIUS*2.5)))
			{
				handTimer = SDL_GetTicks();
				handflag = true;
				mvmtStarted = true;
				std::cerr << "Mvmt Started: " << float(SDL_GetTicks()) << std::endl;
			}

			//if we have started moving, turn off the display for the remainder of the state. we can also calculate the latency now
			if (mvmtStarted) //(handflag)
			{
				startCircle->On();
				targCircle->On();
				drawstruc.drawspath = -1;
				drawstruc.drawtpath[0] = -1;
				drawstruc.drawtpath[1] = -1;
				//drawstruc.drawtrace = -1;
			}


			//note if the subject intersected either path (on the path or has crossed the path)
			if (spath[curtr.spath].OnPath(player,curtr.startx,curtr.starty) || spath[curtr.spath].PathCollision(player,curtr.startx,curtr.starty) )
			{
				intersectstartpath = true;
			}

			if (tpath[curtr.tpath][0].OnPath(player,curtr.xpos,curtr.ypos) || tpath[curtr.tpath][0].PathCollision(player,curtr.xpos,curtr.ypos) )
			{
				//std::cerr << "On target path." << std::endl;
				intersecttgtpath = true;
			}


			//note if/the time when the subject entered the target
			if (player->Distance(targCircle) < TARGET_RADIUS*2){
				if(!targetHit){
					targetHit = true;
					targetTime = SDL_GetTicks();
				}
			}
			else
				targetHit = false;  //if you leave the target at any time, invalidate the flag

			if (player->Distance(targCircle) < TARGET_RADIUS*3)
				targetClose = true;

			if (!mvmtEnded && mvmtStarted && (player->GetVel() < VEL_MVT_TH) && (handTimer-SDL_GetTicks())>200 ) //&& player->(startCircle)>4*START_RADIUS) <- if it is a stop-signal trial the movement may be really short
				{
					mvmtEnded = true;
					mvmtTimer = SDL_GetTicks();
					std::cerr << "Mvmt Ended: " << float(SDL_GetTicks()) << std::endl;
				}
			
			if (player->GetVel() >= VEL_MVT_TH)  //update flag
			{
				mvmtEnded = false;
				mvmtTimer = SDL_GetTicks();
			}

			//if the trial duration is exceeded (waittime msec to move after the last beep) or if you are on the target, or if this is a stop-signal trial and you never moved, end the trial
			if ( (SDL_GetTicks() - gameTimer) > WAITTIME || (mvmtEnded && (SDL_GetTicks()-mvmtTimer)>VEL_END_TIME ) || (curtr.stoptime != 0 && !mvmtStarted && (SDL_GetTicks()-gameTimer)>HOLDTGTTIME ) )   //(targetHit && (SDL_GetTicks()-targetTime) > HOLDTGTTIME) )
			{
				
				dataEndIndex = dataIndex;

				//make sure the visual display is shut off, even if for only 1 sample
				drawstruc.drawspath = -1;
				targCircle->On();
				drawstruc.drawtpath[0] = -1;
				drawstruc.drawtpath[1] = -1;
				drawstruc.drawhandpath = false;
				//drawstruc.drawtrace = -1;

				//check latency: must be within 1000 msec of trial start, and must be positive!
				if (handflag)
					Target.lat = handTimer - fourthBeep;  //latency is the time the hand started moving minus the time the go cue occured
				else
					Target.lat = 5*IBI;

				if (Target.lat < -5*IBI)
					Target.lat = 5*IBI;
				if ( (abs(Target.lat) > LATTIME) && (curtr.stoptime == 0) )
					badlat = true;  //mark latency as bad if it was too early or too late, only if this was not a stop-signal trial

				if ( (Target.lat > LATTIME) && (curtr.stoptime == 0) )
					startCircle->SetColor(lateColor); //late latency
				else if ( (Target.lat < -LATTIME) && (curtr.stoptime == 0) )
					startCircle->SetColor(earlyColor); //early latency
				else if (curtr.stoptime == 0)
					startCircle->SetColor(ontimeColor); //on-time latency

				//score the trial if all flags are set
				if ((!badlat && targetHit && !intersectstartpath && !intersecttgtpath && curtr.stoptime == 0) || (curtr.stoptime != 0 && !mvmtStarted) )  //!exceededtgtflag &&
				{
					scorebeep->Play();
					score += 5;     //5 points for hitting the target correctly
					trscore = 5;

					Target.score = 1;
					targetHit = false;
					//latencyscore = false;
				}
				else
				{
					//display text messages
					if (intersectstartpath && curtr.TrialType != 2)
						drawstruc.drawtext[1] = 1;
					if (intersecttgtpath && curtr.TrialType != 2)
						drawstruc.drawtext[2] = 1;
					if (badlat)
					{
						drawstruc.drawtext[3] = 1;
					}
					if (!targetHit && curtr.TrialType != 2)
						drawstruc.drawtext[5] = 1;
					if (curtr.stoptime != 0 && mvmtStarted)
					{
						drawstruc.drawtext[4] = 1;
						//shut off all the other error feedback, because this was a stop-signal trial so it doesn't matter what the movement looked like
						drawstruc.drawtext[1] = 0;
						drawstruc.drawtext[2] = 0;
						drawstruc.drawtext[3] = 0;
						drawstruc.drawtext[5] = 0;
					}

					//calculate score
					if (curtr.stoptime != 0 && mvmtStarted)  //didn't stop on a stop-signal trial
					{
						stopbeep->Play(); //play error beep

						trscore = -3;
						score -= 3;  //minus 3 points for not obeying the stop signal
						Target.score = -3;

						startCircle->SetDiameter(6*START_RADIUS);
					} 
					else if (badlat)  //started with the wrong latency
					{
						errorbeep->Play(); //play error beep

						trscore = -5;
						score -= 5;  //minus 3 points for not starting the trial on the go cue -- emphasize task timing structure!
						Target.score = -5;
						startCircle->SetDiameter(4*START_RADIUS);
					}
					else if (!targetHit && curtr.TrialType != 2)  //right latency, but did not hit the target (e.g., didn't finish the trial)
					{
						//errorbeep->Play(); //play error beep
						
						trscore = -1;
						score -= 1;  //minus 2 points for not reaching the target -- to emphasize trial completion!
						Target.score = -1;
					}
					else //if (intersectstartpath || intersecttgtpath)  //hit the target with the right latency, but hit one of the barriers
					{
						trscore = 0;
						Target.score = 0;
					}

				}
				drawstruc.drawtext[10] = trscore;


				if (intersectstartpath && curtr.stoptime == 0 )
					spath[curtr.spath].SetPathColor(HitPathClr);
				if(intersecttgtpath && curtr.stoptime == 0)
					tpath[curtr.tpath][0].SetPathColor(HitPathClr);


				if (curtr.stoptime != 0)
				{
					rotspath = curtr.spath-curtr.tgt;
					rotspath = (rotspath>=0 ? rotspath : rotspath+8);

					rottpath = curtr.tpath-curtr.tgt;
					rottpath = (rottpath>=0 ? rottpath : rottpath+8);

					Stairs[rotspath][rottpath].AddDataPoint(curtr.stoptime,int(!mvmtStarted),Target.lat);
				}
				

				Target.trial = 0;
				Target.stop = 0;

				std::cerr << "Trial " << CurTrial+1 << " ended at " << SDL_GetTicks() 
					<< ". Elapsed time, " << (SDL_GetTicks() - TstartTrial) << std::endl;
				
				gameTimer = SDL_GetTicks(); //time the trial ends

				//move to the ShowResult state
				std::cerr << "Leaving ACTIVE state to SHOWRESULT state." << std::endl;
				hoverTimer = SDL_GetTicks();
				state = ShowResult;

			}
			
			break;
		
		case ShowResult:

			Target.trial = 0;
			Target.stop = 0;

			drawstruc.drawhandpath = true;

			indx = (birds_connected ? 3 : 0);
			drawstruc.drawbird[indx] = 1;

			//drawstruc.drawstart = true;
			targCircle->On();
			drawstruc.drawspath = curtr.spath;
			drawstruc.drawtpath[0] = curtr.tpath;
			//drawstruc.drawtrace = curtr.trace;


			if (SDL_GetTicks() - gameTimer > SHOWRESULTTIME)
			{

				CurTrial++;

				drawstruc.drawspath = -1;
				drawstruc.drawtpath[0] = -1;
				drawstruc.drawtpath[1] = -1;
				//drawstruc.drawtrace = -1;
				startCircle->Off();
				startCircle->SetDiameter(2*START_RADIUS);
				targCircle->Off();

				for (a = 0; a < 10; a++)
					drawstruc.drawtext[a] = 0;
				drawstruc.drawtext[10] = -99;

				//if we have reached the end of the trial table, quit
				if (CurTrial >= NTRIALS)
				{
					std::cerr << "Leaving SHOWRESULT state to FINISHED state." << std::endl;
					gameTimer = SDL_GetTicks();
					state = Finished;
				}
				else
				{
					std::cerr << "Leaving SHOWRESULT state to STARTING state." << std::endl;
					hoverTimer = SDL_GetTicks();
					state = Starting;
				}
			}

			break;
		
		case Finished:
			// Trial table ended, wait for program to quit

			//drawstruc.drawhandpath = false;
			targCircle->Off();
			startCircle->Off();
			drawstruc.drawspath = -1;
			drawstruc.drawtpath[0] = -1;
			drawstruc.drawtpath[1] = -1;
			//drawstruc.drawtrace = -1;
			drawstruc.drawhandpath = false;
			//drawstruc.drawvelbar = -1;
			Target.score = -1;
			Target.lat = -5000;
		
			//drawstruc.drawtext = true;
			drawstruc.drawtext[0] = 1;
			for (a = 1; a < 10; a++)
				drawstruc.drawtext[a] = 1;
			drawstruc.drawtext[4] = -99;

			break;
			


	} //end switch(state)

} //end game_update

