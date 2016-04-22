#include <cmath>
#include <sstream>
#include <iostream>
#include <fstream>
#include <istream>
#include <windows.h>
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include "InputFrame.h"
#include "DataWriter.h"
#include "Circle.h"
#include "HandCursor.h"
#include "TrackBird.h"
#include "Object2D.h"
#include "Sound.h"
#include "Path2D.h"
#include "PhotoSensor.h"
//#include "Region2D.h"

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
		
*/


//state machine
enum GameState
{
	Idle = 0x01,		//00001
	Starting = 0x03,	//00011
	HoldTraj = 0x04,		//00100
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
Circle* cursor[BIRDCOUNT + 1];
HandCursor* curs[BIRDCOUNT + 1];
HandCursor* player = NULL;
Circle* startCircle = NULL;
Circle* targCircle = NULL;
Circle* photosensorCircle = NULL;
Object2D* targettraces[NTRACES];
Image* VelBarFrame;
Image* VelBar;
Image* text = NULL;
Image* text1 = NULL;
Image* text2 = NULL;
Image* text3 = NULL;
Image* text4 = NULL;
Image* trialnum = NULL;
Sound* scorebeep = NULL;
Sound* startbeep = NULL;
Sound* errorbeep = NULL;
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

Path2D spath[8];  //barriers at each of the 8 possible orientations around the start target
Path2D tpath[8];  //barriers at each of the 8 possible orientations around the reach target

//variables to show hand path (KR)
float trialData[10000][2];
int dataIndex = 0;
int dataEndIndex = 0;

//tracker variables
int trackstatus;
TrackSYSCONFIG sysconfig;
TrackDATAFRAME dataframe;
Uint32 DataStartTime = 0;

//velocity-tracking variables
float PeakVel;

//colors
float cursColor[3] = {1.0f, 0.0f, 0.0f};
float startColor[3] = {.6f,.6f,.6f};
float targetColor[3] = {0.0f, 0.5f, 1.0f};
float blkColor[3] = {0.0f, 0.0f, 0.0f};
float whiteColor[3] = {1.0f, 1.0f, 1.0f};
float redColor[3] = {1.0f, 0.0f, 0.0f};
float greenColor[3] = {0.0f, 1.0f, 0.0f};

//FTDI photosensor variables
FT_HANDLE ftHandle;
bool sensorsActive;


//variables to compute the earned score
int score = 0;


// Trial table structure, to keep track of all the parameters for each trial of the experiment
typedef struct {
	int TrialType;			// Flag for type of trial: 1 = RT task, 4 = RT task with no feedback on this trial
	float startx,starty;	// x/y pos of start target; also, trace will be centered here!
	float xpos,ypos;		// x/y pos of target1.
	int spath;				//start target barrier to display (centered on start target - use pathx and pathy)
	int tpath;				//target barrier to display (centered on reach target - use pathx and pathy)
	int trace;				//trace (>= 0) for cuing how to draw the path, or -1 otherwise.
	int iti;				//hold time before trial begins
	int TMStime;			//time after stim onset that a TMS pulse will be delivered, or -1 otherwise.
} TRTBL;

#define TRTBL_SIZE 1000
TRTBL trtbl [TRTBL_SIZE];


int NTRIALS = 0;
int CurTrial = 0;

#define curtr trtbl[CurTrial]

//target structure; keep track of where the current target is now (only 1!)
TargetFrame Target;

//structure to keep track of what to draw in the draw_screen() function
typedef struct {
	int drawtrace;				  //trace number to draw (for intro screen)
	int drawspath;                //path number to draw
	int drawtpath;                //path number to draw
	bool drawhandpath;			  //draw saved hand path (KR)
	int drawtext[5];                //write feedback text, depending on what flags are set
	float drawvelbar;             //velocity feedback-bar parameter
} DRAWSTRUC;

DRAWSTRUC drawstruc;

// Initializes everything and returns true if there were no errors
bool init();
// Sets up OpenGL
void setup_opengl();
// Performs closing operations
void clean_up();
// Draws objects on the screen
void draw_screen();

//file to load in trial table
int LoadTrFile(char *filename);

// Update loop (state machine)
void game_update();

//send trigger signal to TMS
void TriggerTMS();
Uint32 TMSTrigTime;
bool didTMStrigger;

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
	
	DataStartTime = SDL_GetTicks();

	bool quit = false;
	while (!quit)
	{
		//game_update(); // Run the game loop

		bool inputs_updated = false;

		// Retrieve Flock of Birds data
		if (trackstatus>0)
		{
			// Update inputs from Ascension
			inputs_updated = TrackBird::GetUpdatedSample(&sysconfig,&dataframe);
		}

		// Handle SDL events
		while (SDL_PollEvent(&event))
		{
			// See http://www.libsdl.org/docs/html/sdlevent.html for list of event types
			if (event.type == SDL_MOUSEMOTION)
			{
				inputs_updated += ((trackstatus>0) ? 0 : 1); // Record data if birds are not connected
				dataframe.x[0] = (GLfloat)event.motion.x * PHYSICAL_RATIO;
				dataframe.y[0] = (GLfloat)(SCREEN_HEIGHT - event.motion.y) * PHYSICAL_RATIO;
				dataframe.z[0] = 0.0f;
				dataframe.time[0] = SDL_GetTicks();
				dataframe.etime[0] = dataframe.time[0]-DataStartTime;
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

			if (sensorsActive)
			{
				UCHAR bit = 2;  //this is the RxD line
				Target.TMStrigger = PhotoSensor::GetSensorBitBang(ftHandle,bit);
			}
			else
				Target.TMStrigger = -99;

			//updatedisplay = true;
			for (int a = ((trackstatus>0) ? 1 : 0); a <= ((trackstatus>0) ? BIRDCOUNT : 0); a++)
			{

				curs[a]->UpdatePos(dataframe.x[a],dataframe.y[a]);

				InputFrame i;
				i.time = dataframe.time[a];
				i.x = dataframe.x[a];
				i.y = dataframe.y[a];
				i.z = dataframe.z[a];

				i.vel = curs[a]->GetVel();
				i.rotx = curs[a]->GetX();
				i.roty = curs[a]->GetY();

				writer->Record(a, i, Target);
			}
			
			//update the data array
			trialData[dataIndex][0] = player->GetX();
			trialData[dataIndex][1] = player->GetY();
			dataIndex = (++dataIndex >= 9999 ? 9999 : dataIndex);   //if we exceed the array, just keep replacing the last data point as "junk"
			
		}

		game_update(); // Run the game loop (state machine update)

		draw_screen();

	}

	clean_up();
	//return 0;
}


//send signal over the FTDI chip to trigger the TMS
void TriggerTMS()
{

	int bit = 3;
	int bitval;

	UCHAR Mask = 0x4f; 

	bitval = PhotoSensor::GetSensorBitBang(ftHandle,bit);

	if (!didTMStrigger && bitval == 1)  //if not already triggered, trigger the pulse
	{
		PhotoSensor::SetSensorBitBang(ftHandle,Mask,bit,0);
		TMSTrigTime = SDL_GetTicks();
		didTMStrigger = true;
	}
	else //we already commanded the bit high, wait 10 ms and then command it low
	{
		if (bitval == 0 && (SDL_GetTicks()-TMSTrigTime) > 10)
			PhotoSensor::SetSensorBitBang(ftHandle,Mask,bit,1);
	}

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

	trfile.getline(tmpline,sizeof(tmpline),'\n');  //get the first line of the file, which is the name of the trial-table file
	
	while(!trfile.eof())
	{
		sscanf(tmpline, "%d %f %f %f %f %d %d %d %d %d %d", 
			&trtbl[ntrials].TrialType, 
			&trtbl[ntrials].startx,&trtbl[ntrials].starty, 
			&trtbl[ntrials].xpos,&trtbl[ntrials].ypos, 
			&trtbl[ntrials].spath, &trtbl[ntrials].tpath, 
			&trtbl[ntrials].trace, &trtbl[ntrials].iti,
			&trtbl[ntrials].TMStime);
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

	std::cerr << std::endl;

	trackstatus = TrackBird::InitializeBird(&sysconfig);
	if (trackstatus <= 0)
		std::cerr << "Tracker failed to initialize. Mouse Mode." << std::endl;

	std::cerr << std::endl;

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
		std::cerr << "  Screen failed to build." << std::endl;
		return false;
	}
	else
		std::cerr << "  Screen built." << std::endl;

	setup_opengl();
	//Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 8, 4096);
	Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 512);  //open a smaller buffer, which addresses timing issues (DMH)
	if (TTF_Init() == -1)
	{
		std::cerr << "  Audio failed to initialize." << std::endl;
		return false;
	}
	else
		std::cerr << "  Audio initialized." << std::endl;

	//turn off the computer cursor
	SDL_ShowCursor(0);

	std::cerr << std::endl;

	// Load files and initialize pointers
	font = TTF_OpenFont("Resources/arial.ttf", 28);
	
	Image* tgttraces[NTRACES+1]; 

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
			//targettraces[a]->SetPos(PHYSICAL_WIDTH / 2, PHYSICAL_HEIGHT / 2);
		}
	}
	
	//load the velocity bar feedback frame
	VelBarFrame = Image::LoadFromFile("Resources/velbarframe.png");
	if (VelBarFrame == NULL)
		std::cerr << "Image VelBarFrame did not load." << std::endl;

	//load the velocity bar
	VelBar = Image::LoadFromFile("Resources/velbar.png");
	if (VelBar == NULL)
		std::cerr << "Image VelBar did not load." << std::endl;

	std::cerr << "Images loaded." << std::endl;
	std::cerr << std::endl;

	startCircle = new Circle(curtr.startx, curtr.starty, START_RADIUS*2, startColor);
	startCircle->setBorderWidth(0.001f);
	startCircle->SetBorderColor(blkColor);
	startCircle->On();
	startCircle->BorderOn();

	targCircle = new Circle(curtr.xpos, curtr.ypos, TARGET_RADIUS*2, targetColor);
	targCircle->SetBorderColor(targetColor);
	targCircle->setBorderWidth(0.002f);
	targCircle->BorderOn();
	targCircle->Off();
	
	photosensorCircle = new Circle(PHYSICAL_WIDTH-0.06,0.20,0.03,blkColor);
	photosensorCircle->SetBorderColor(blkColor);
	photosensorCircle->BorderOff();
	photosensorCircle->On();

	//load the path files
	for (a = 0; a < NPATHS; a++)
	{
		sprintf(tmpstr,"%s/Path%d.txt",PATHPATH,a); 
		spath[a] = Path2D::LoadPathFromFile(tmpstr);
		tpath[a] = Path2D::LoadPathFromFile(tmpstr);
		if (spath[a].GetPathNVerts() < 0)
			std::cerr << "   Path " << a << " did not load." << std::endl;
		else
			std::cerr << "   Path " << a << " loaded." << std::endl;
	}

	std::cerr << std::endl;

	//initialize the photosensor
	int status = -5;
	int devNum = 0;

	FT_STATUS ftStatus; 
	DWORD numDevs;

	UCHAR Mask = 0x4f;  
	//the bits in the upper nibble should be set to 1 to be output lines and 0 to be input lines (only used 
	//  in SetSensorBitBang() ). The bits in the lower nibble should be set to 1 initially to be active lines.

	status = PhotoSensor::InitSensor(devNum,&ftHandle,1,Mask);
	std::cerr << "PhotoSensor: " << status << std::endl;

	status = PhotoSensor::SetSensorBitBang(ftHandle,Mask,3,0);

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

	std::cerr << std::endl;

	//load trial table from file
	NTRIALS = LoadTrFile(fname);
	//std::cerr << "Filename: " << fname << std::endl;
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


	// set up the cursors
	if (trackstatus > 0)
	{
		/* Assign birds to the same indices of controller and cursor that they use
		* for the Flock of Birds
		*/
		for (a = 1; a <= BIRDCOUNT; a++)
		{
			cursor[a] = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*2, cursColor);
			cursor[a]->BorderOff();
			curs[a] = new HandCursor(cursor[a]); 
			curs[a]->SetOrigin(curtr.startx, curtr.starty);
		}

		player = curs[HAND];  //this is the cursor that represents the hand

	}
	else
	{
		// Use mouse control
		cursor[0] = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*2, cursColor);
		curs[0] = new HandCursor(cursor[0]);
		curs[0]->SetOrigin(curtr.startx, curtr.starty);
		player = curs[0];
	}

	PeakVel = -1;


	//load sound files
	scorebeep = new Sound("Resources/correctbeep.wav");
	startbeep = new Sound("Resources/startbeep.wav");
	errorbeep = new Sound("Resources/errorbeep1.wav");


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
	textstring << "You need to begin your movement sooner.";
	text3 = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "Wait for the GO cue to begin your movement!";
	text4 = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	trialnumfont = TTF_OpenFont("Resources/arial.ttf", 12);
	trialnum = new Image(TTF_RenderText_Blended(trialnumfont, "1", textColor));
	
	SDL_WM_SetCaption("RTboxTask", NULL);

	
	// Set the initial game state
	state = Idle; 


	std::cerr << "initialization complete." << std::endl << std::endl;
	std::cerr << "~~~" << std::endl << std::endl;
	return true;
}

float lineWidth[2];

static void setup_opengl()
{
	glClearColor(1.0f, 1.0f, 1.0f, 0);

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
	
	delete startCircle;
	delete targCircle;
	delete scorebeep;
	delete startbeep;
	
	int status = PhotoSensor::CloseSensor(ftHandle,1);

	delete text;
	delete text1;
	delete text2;
	delete text3;
	delete text4;

	delete writer;
	Mix_CloseAudio();
	TTF_CloseFont(font);
	TTF_Quit();
	SDL_Quit();

	if (trackstatus > 0)
		TrackBird::ShutDownBird(&sysconfig);
}

//control what is drawn to the screen
static void draw_screen()
{
	int a;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	
	//draw the trace specified
	if (drawstruc.drawtrace >= 0)
	{
		//targettraces[drawstruc.drawtrace]->Draw(PHYSICAL_WIDTH, PHYSICAL_HEIGHT);
		targettraces[drawstruc.drawtrace]->SetPos(curtr.startx,curtr.starty);
		targettraces[drawstruc.drawtrace]->Draw();
		Target.trace = drawstruc.drawtrace;
	}
	else
		Target.trace = -1;
	
	
	//draw the velocity feedback bar, unless we have reached the end of the block (write text instead)
	if ((drawstruc.drawvelbar >= 0) && (drawstruc.drawtext[0]==0))
	{
		VelBarFrame->Draw(PHYSICAL_WIDTH/2,PHYSICAL_HEIGHT*0.9);
		VelBar->DrawAlign(PHYSICAL_WIDTH/2-(VelBarFrame->GetWidth()/2),PHYSICAL_HEIGHT*0.9,VelBar->GetWidth()*drawstruc.drawvelbar,VelBar->GetHeight(),0.0f,-1);	
	}
	

	//draw the path
	if (drawstruc.drawspath >= 0)
	{
		spath[drawstruc.drawspath].Draw(curtr.startx,curtr.starty);
		Target.spath = drawstruc.drawspath;
	}
	else
		Target.spath = -1;

	if (drawstruc.drawtpath >= 0)
	{
		//tpath[drawstruc.drawtpath].Draw(target->GetX(),target->GetY());
		tpath[drawstruc.drawtpath].Draw(curtr.xpos,curtr.ypos);
		Target.tpath = drawstruc.drawtpath;
	}
	else
		Target.tpath = -1;

	//draw the hand path, if requested
	if (drawstruc.drawhandpath)
	{
		// Draw path
		glDisable(GL_TEXTURE_2D);
		glColor3f(1.0f,0.0f,0.0f);
		//glLineWidth((CURSOR_RADIUS*2.5)/PHYSICAL_RATIO);
		glLineWidth(6.5f);
		//glLineWidth(min((CURSOR_RADIUS*2)/PHYSICAL_RATIO,lineWidth[1]));

		glBegin(GL_LINE_STRIP);

		for (int a = 0; a<=dataEndIndex; a++)
		{
			if (a < 3 || a >= dataEndIndex-3)
				glVertex3f(trialData[a][0],trialData[a][1],0.0f);
			else //smooth out the line, since it is unfiltered data.
			{
				float xdat = (trialData[a-2][0]+trialData[a-1][0]+trialData[a][0]+trialData[a+1][0]+trialData[a+2][0])/5;
				float ydat = (trialData[a-2][1]+trialData[a-1][1]+trialData[a][1]+trialData[a+1][1]+trialData[a+2][1])/5;
				glVertex3f(xdat,ydat,0.0f);
			}
		}

		glEnd();

		//reset defaults after the draw
		glColor3f(1.0f,1.0f,1.0f);
		glLineWidth(1.0f);

	}
	

	// Draw the start marker, if true
	startCircle->Draw();
	if (startCircle->drawState())
	{
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
			text->Draw(0.6f, 0.5f);
			for (a=1; a<5; a++)
				drawstruc.drawtext[a] = 0;
	}
	if (drawstruc.drawtext[1] == 1)
	{
		text1->Draw(0.6f,0.6f); //hit start barrier
	}
	if (drawstruc.drawtext[2] == 1)
		text2->Draw(0.6f,0.57f); //hit target barrier
	if (drawstruc.drawtext[3] == 1)
		text3->Draw(0.6f,0.55f); //too late
	if (drawstruc.drawtext[4] == 1)
		text4->Draw(0.6f,0.53f); //wrong timing

	trialnum->Draw(PHYSICAL_WIDTH*23/24, PHYSICAL_HEIGHT*23/24);

	SDL_GL_SwapBuffers();
	glFlush();

	//updatedisplay = false;
}

//game update loop - state machine controlling the status of the experiment
bool reachedtgtflag = false;	//flag to see if the subject reached the target
bool exceededtgtflag = false;	//flag to see if the subject reached the target
//bool holdtgtflag = false;		//flag to see if the subject held still on the target
bool handflag = false;			//flag to see if the hand started moving (for use with calculating latency)
bool latencyscore = false;		//flag to see if the latency is ok
bool handtrigger = false;

bool intersectstartpath = false; //flag to see if the start path has been intersected
bool intersecttgtpath = false;	 //flag to see if the target path has been intersected

bool dispOff = false;  //turn off display during the Active state if already been in the HoldTraj state
bool latflag = false;  //flag to detect early start before the go cue
int lattime = 1200;    //acceptable maximum latency (depends on TrialType)

bool mvmtEnded = true;
Uint32 mvmtTimer;


Uint32 handTimer;				//timer to save the onset time of the hand movement - for latency calculation

//bool RTvalid = false;
//bool didjump = false;			//flag to indicate if the target jump has already occurred or not.

int indx;

float LastPeakVel = 0;

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
			//drawstruc.drawtrace = 0;  //for calibration purposes, display trace0 always
			startCircle->SetPos(curtr.startx, curtr.starty);
			startCircle->On();
			drawstruc.drawspath = -1;
			drawstruc.drawtpath = -1;
			targCircle->Off();
			for (a = 0; a < 5; a++)
				drawstruc.drawtext[a] = 0;

			drawstruc.drawhandpath = false;
			dataIndex = 0;
			drawstruc.drawvelbar = -1;
			Target.score = -1;
			Target.lat = -5000;
			drawstruc.drawtrace = -1;

			//if cursor is in the start target, move to Starting state.
			if( (player->Distance(startCircle) <= START_RADIUS) && (CurTrial < NTRIALS) )
			{
				//targettraces[curtr.trace]->SetPos(curtr.startx, curtr.starty);
				//drawstruc.drawtrace = curtr.trace;
				//drawstruc.drawtrace = -1;  //no trace until the first beep
				photosensorCircle->On();
				hoverTimer = SDL_GetTicks();
				gameTimer = SDL_GetTicks();
				std::cerr << "Leaving IDLE state." << std::endl;
				LastPeakVel = PeakVel;
				PeakVel = 0;
				handflag = false;
				latflag = false;
				Target.score = -1;
				Target.lat = -5000;
				intersectstartpath = false;
				intersecttgtpath = false;
				didTMStrigger = false;

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
			//starttgt->SetPos(curtr.startx, curtr.starty);
			startCircle->On();
			targCircle->Off();
			photosensorCircle->On();
			//drawstruc.drawpath = curtr.path1;
			drawstruc.drawspath = -1;
			drawstruc.drawtpath = -1;
			drawstruc.drawtrace = -1;
			for (a = 0; a < NPATHS; a++)
			{
				spath[a].SetPathColor(GoodPathClr);
				tpath[a].SetPathColor(GoodPathClr);
			}
			drawstruc.drawhandpath = false;
			dataIndex = 0;
			for (a = 0; a < 5; a++)
				drawstruc.drawtext[a] = 0;

			/*
			drawstruc.drawvelbar = (LastPeakVel-VELMIN+(VELMAX-VELMIN)/2)/(2*(VELMAX-VELMIN));  //draw the velocity feedback bar
			drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0 : drawstruc.drawvelbar);
			drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1 : drawstruc.drawvelbar);
			*/


			if (player->Distance(startCircle) > START_RADIUS)
			{
				state = Idle;
				Target.score = -1;
				Target.lat = -5000;
			}
			// If player hovers long enough, set state to Active
			else if (SDL_GetTicks() - hoverTimer >= curtr.iti)
			{
				//targettraces[curtr.trace]->SetPos(curtr.startx, curtr.starty);
				//drawstruc.drawtrace = curtr.trace;
				targCircle->SetPos(curtr.xpos, curtr.ypos);
				targCircle->On();
				photosensorCircle->Off();
				
				if (curtr.TrialType != 3 || curtr.trace == -1)
				{
					drawstruc.drawspath = curtr.spath;
					drawstruc.drawtpath = curtr.tpath;
				}
				drawstruc.drawtrace = curtr.trace;
				
				reachedtgtflag = false;
				exceededtgtflag = false;
				handflag = false;
				latflag = false;
				latencyscore = false;
				handtrigger = false;

				dispOff = false;

				mvmtEnded = false;

				TstartTrial = SDL_GetTicks();

				Target.score = -1;
				Target.lat = -5000;

				didTMStrigger = false;

				intersectstartpath = false;
				intersecttgtpath = false;
				startbeep->Play();
				gameTimer = SDL_GetTicks();
				std::cerr << "Leaving STARTING state." << std::endl;
				//if (curtr.TrialType == 3)
				//	state = HoldTraj;
				//else
				
				
				state = Active;
			}

			break;

			/*
		case HoldTraj:

			startCircle->On();
			drawstruc.drawhandpath = false;

			if (!latflag && (SDL_GetTicks() - gameTimer < FIXTIME) && (player->Distance(startCircle) > START_RADIUS*1.5))
			{
				targCircle->On();
				drawstruc.drawtext[4] = 1;
				drawstruc.drawspath = -1;
				drawstruc.drawtpath = -1;
				drawstruc.drawtrace = -1;

				latflag = true;
				errorbeep->Play();
				handTimer = SDL_GetTicks();
			}

			if (latflag && (SDL_GetTicks() - handTimer > FIXTIME))
			{
				std::cerr << "Restarting Trial - Broke Fixation." << std::endl;
				hoverTimer = SDL_GetTicks();
				state = Starting;
			}

			if (!latflag && (SDL_GetTicks() - gameTimer > DISPTIME))  //if surpass the Display time, shut off the display
			{
				dispOff = true;

				targCircle->On();
				drawstruc.drawspath = -1;
				drawstruc.drawtpath = -1;
				drawstruc.drawtrace = -1;
			}

			if (!latflag && (SDL_GetTicks() - gameTimer > FIXTIME))  //if surpass the fixation time, cue the go signal
			{
				gameTimer = SDL_GetTicks();

				startbeep->Play();

				std::cerr << "Leaving HOLDTRAJ state." << std::endl;
				state = Active;
			}

			break;
			*/

		case Active:
			
			//targettraces[curtr.trace]->SetPos(curtr.startx, curtr.starty);
			//drawstruc.drawtrace = curtr.trace;
			startCircle->On();
			drawstruc.drawhandpath = false;
			dataEndIndex = dataIndex;
			
			//drawstruc.drawvelbar = -1;
			if (SDL_GetTicks()-gameTimer > curtr.TMStime)
			{
				TriggerTMS();
			}
			//check if the pulse is over; only then, allow the other flags to be set
			if (SDL_GetTicks()-TMSTrigTime > TMSWAITTIME )
				handtrigger = true;


			//detect the onset of hand movement, for calculating latency
			if (handtrigger && !handflag && (player->Distance(startCircle) > START_RADIUS*1.5))
			{
				handTimer = SDL_GetTicks();
				handflag = true;
				Target.lat = handTimer - gameTimer;
				gameTimer = handTimer;  //time the trial from movement onset!
			}
			
			
			if (handtrigger && (player->Distance(startCircle) >= START_RADIUS*1.5) || dispOff)
			{
				//shut off the visual display during the movement
				targCircle->Off();
				drawstruc.drawspath = -1;
				drawstruc.drawtpath = -1;
				drawstruc.drawtrace = -1;
				dispOff = true;
			}
			else
			{
				targCircle->On();
				drawstruc.drawspath = curtr.spath;
				drawstruc.drawtpath = curtr.tpath;
				drawstruc.drawtrace = curtr.trace;
			}
			

			/*
			//shut off the hand cursor during the trial?
			if ((player->(startCircle) > START_RADIUS*3))
			{
				player->Off();
			}
			*/


			//note if the subject intersected either path
			if (handtrigger && curtr.spath >= 0 && spath[curtr.spath].OnPath(player,curtr.startx,curtr.starty))
			{
				//std::cerr << "  On start path.      " ;
				intersectstartpath = true;
			}
			if (handtrigger && curtr.tpath >= 0 && tpath[curtr.tpath].OnPath(player,curtr.xpos,curtr.ypos))
			{
				//std::cerr << "On target path." << std::endl;
				intersecttgtpath = true;
			}

			//note if the subject entered the target
			if (handtrigger && player->Distance(targCircle) < TARGET_RADIUS*2)
			{
				reachedtgtflag = true;
			}
			
			if (handtrigger && reachedtgtflag && player->Distance(targCircle) >= TARGET_RADIUS*5)
				exceededtgtflag = true;

			//keep track of the maximum (peak) velocity
			if (handtrigger && (player->GetVel() > PeakVel) && !reachedtgtflag)
				PeakVel = player->GetVel();

			if (handtrigger && !mvmtEnded && handflag && (player->GetVel() < VEL_MVT_TH) && (handTimer-SDL_GetTicks())>200 && player->Distance(startCircle)>4*START_RADIUS)
				{
					mvmtEnded = true;
					mvmtTimer = SDL_GetTicks();
					std::cerr << "Mvmt Ended: " << float(SDL_GetTicks()) << std::endl;
				}

			if (handtrigger && (player->GetVel() >= VEL_MVT_TH))
			{
				mvmtEnded = false;
				mvmtTimer = SDL_GetTicks();
			}


			//if the trial duration is exceeded (waittime msec to move after the last beep) or the hand has stopped moving, end the trial
			if ( ((SDL_GetTicks() - gameTimer) > WAITTIME) || (mvmtEnded && (SDL_GetTicks()-mvmtTimer)>VEL_END_TIME ) )
			{
				
				//make sure the visual display is shut off, even if for only 1 sample
				targCircle->Off();
				drawstruc.drawspath = -1;
				drawstruc.drawtpath = -1;
				drawstruc.drawhandpath = false;
				drawstruc.drawtrace = -1;

				if (curtr.TrialType != 4)  //if Trtype == 4, no feedback at end of trial.
				{

					//change the path color if it was hit
					if (intersectstartpath)
						spath[curtr.spath].SetPathColor(HitPathClr);
					if(intersecttgtpath)
						tpath[curtr.tpath].SetPathColor(HitPathClr);
				}

				//check latency: must be within 1000 msec of trial start, and must be positive!
				if (Target.lat < -4000)
					Target.lat = -2000;

				//if ( (abs((int)handTimer - (int)gameTimer) < 1200))
				if (curtr.TrialType == 3)
					lattime = 700;
				else
					lattime = 1200;

				if (abs(Target.lat) < lattime)
				{
					latencyscore = true;
				}
				else
					latencyscore = false;

				//score the trial if all flags are set
				if (latencyscore && reachedtgtflag && !intersectstartpath && !intersecttgtpath && !exceededtgtflag)  //!exceededtgtflag &&
				{
					scorebeep->Play();
					score++;     //individual target score
					Target.score = 1;
					reachedtgtflag = false;
					exceededtgtflag = false;
					latencyscore = false;
				}
				else if (curtr.TrialType == 4)
				{
					if (!latencyscore)
					{
						drawstruc.drawtext[3] = 1;
						errorbeep->Play();
					}
					else
					{
						scorebeep->Play();
						Target.score = 0;
					}
					reachedtgtflag = false;
					exceededtgtflag = false;
					latencyscore = false;
				}

				else
				{
					if (intersectstartpath)
						drawstruc.drawtext[1] = 1;
					if (intersecttgtpath)
						drawstruc.drawtext[2] = 1;
					if (!latencyscore)
					{
						drawstruc.drawtext[3] = 1;
						errorbeep->Play();
					}
					
					Target.score = 0;
				}
				
				std::cerr << "Trial " << CurTrial+1 << " ended at " << SDL_GetTicks() 
					<< ". Elapsed time, " << (SDL_GetTicks() - TstartTrial) << std::endl;
				
				LastPeakVel = PeakVel;
				PeakVel = 0;

				gameTimer = SDL_GetTicks(); //time the target is achieved

				//if we have reached the end of the trial table, quit
				if (CurTrial+1 >= NTRIALS)
				{
					std::cerr << "Leaving ACTIVE state to FINISHED state." << std::endl;
					//gameTimer = SDL_GetTicks();
					//state = Finished;
					//CurTrial--;
					state = ShowResult;
				}
				else
				{
					std::cerr << "Leaving ACTIVE state to STARTING state." << std::endl;
					hoverTimer = SDL_GetTicks();
					//state = Starting;
					//CurTrial--;
					state = ShowResult;
				}


			}
			

			break;
		
		case ShowResult:

			if (curtr.TrialType != 4)
			{

				drawstruc.drawhandpath = true;

				player->On();

				startCircle->On();
				targCircle->On();
				drawstruc.drawspath = curtr.spath;
				drawstruc.drawtpath = curtr.tpath;
				drawstruc.drawtrace = curtr.trace;
			}
			else
			{
				drawstruc.drawhandpath = false;
				startCircle->On();
				targCircle->Off();
				drawstruc.drawspath = -1;
				drawstruc.drawtpath = -1;
				drawstruc.drawtrace = -1;
			}


			if (SDL_GetTicks() - gameTimer > 1200)
			{

				CurTrial++;

				std::stringstream texttn;
				texttn << CurTrial+1;  //CurTrial starts from 0, so we add 1 for convention.
				delete trialnum;
				trialnum = new Image(TTF_RenderText_Blended(trialnumfont, texttn.str().c_str(), textColor));

				drawstruc.drawspath = -1;
				drawstruc.drawtpath = -1;
				drawstruc.drawtrace = -1;
				startCircle->Off();
				targCircle->Off();

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
				startCircle->Off();
				targCircle->Off();
			//drawstruc.drawtrace = -1;
			drawstruc.drawspath = -1;
			drawstruc.drawtpath = -1;
			drawstruc.drawtrace = -1;
			drawstruc.drawhandpath = false;
			//drawstruc.drawvelbar = -1;
			Target.score = -1;
			Target.lat = -5000;
		
			//drawstruc.drawtext = true;
			drawstruc.drawtext[0] = 1;
			for (a = 1; a < 5; a++)
				drawstruc.drawtext[a] = 1;

			break;
			


	} //end switch(state)

} //end game_update

