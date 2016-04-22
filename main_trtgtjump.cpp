#include <cmath>
#include <sstream>
//#include <iomanip>
#include <iostream>
#include <fstream>
#include <istream>
//#include <numeric>
#include <windows.h>
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include "DataWriter.h"
#include "Circle.h"
#include "HandCursor.h"
//#include "MouseInput.h"
#include "TrackBird.h"
#include "Object2D.h"
//#include "Region2D.h"
#include "PhotoSensor.h"
#include "Sound.h"

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
	Timed response paradigm.
		- created new foleder kinereach_cppcode_TRbase for base-line timed response paradigm
		- renamed project and solution to timedResponse
		
		Structural Changes:
		- Only display a single target on the screen at a time.
		- During Idle state (before trial commencement) display no targets and only the center circle.
		- During Starting state (during start circle hover) display no targets and only center circle (still).
		- During Active state (tones and movement) display a single target at location 1 and possibly a second target at loc 2 if this is a jump trial
		- During Feedback state (show result of trial) display the last target that was shown, and 
		
		Mods by DMH - 04/16/14:

*/


//state machine
enum GameState
{
	Idle = 0x01,       //00001
	Starting = 0x03,   //00011
	Active = 0x06,     //00110
	ShowResult = 0x08, //01000
	Ending = 0x0C,     //01100
	Finished = 0x10    //10000
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
//Object2D* targettraces[NTRACES];
//Region2D barrierset[NTRACES];
//float barrierColor[3] = {.5, .5, .5};
//float verts[3][2] = {{.605, .230}, {.552, .283}, {.578, .309}};
Image* text = NULL;
Image* textTooLate = NULL;
Image* textTooEarly = NULL;
Image* textFailVelCrit = NULL;
Image* trialnum = NULL;
Sound* scorebeep = NULL;
Sound* synchrobeep = NULL;
Sound* errorbeep = NULL;
//Sound* barrierbeep = NULL;
TTF_Font* font = NULL;
TTF_Font* trialnumfont = NULL;
SDL_Color textColor = {0, 0, 0};
DataWriter* writer = NULL;
GameState state;
Uint32 gameTimer;
Uint32 hoverTimer;

//velocity-tracking variables
float PeakVel;
Image* VelBarFrame;
Image* VelBarWin;
Image* VelBar;
float VminShow,VmaxShow;


//Photosensor variables
FT_HANDLE ftHandle;
bool sensorsActive;

//tracker variables
int trackstatus;
TrackSYSCONFIG sysconfig;
TrackDATAFRAME dataframe;
Uint32 DataStartTime = 0;

//variables to compute the earned score
int score = 0;

//constants for Timed Response timing:
//int INTER_TONE_INTERVAL = 300;
int currToneRingNum = 0;
Uint32 lastToneRing;

//colors
float cursColor[3] = {0.0f, 0.0f, 1.0f};
float startColor[3] = {0.6f, 0.6f, 0.6f};
float targetFastColor[3] = {0.0f, 1.0f, 0.0f};
float targetSlowColor[3] = {1.0f, 0.27f, 0.0f};
float targHitColor[3] = {0.0f, 0.5f, 1.0f};
float blkColor[3] = {0.0f, 0.0f, 0.0f};
float whiteColor[3] = {1.0f, 1.0f, 1.0f};
float redColor[3] = {5.0f, 0.0f, 0.0f};
float greenColor[3] = {0.0f, 5.0f, 0.0f};


// Trial table structure, to keep track of all the parameters for each trial of the experiment
typedef struct {
	//int TrialType;		// Flag 1-for new trial.
	float startx,starty;	// x/y pos of start target; also, trace will be centered here!
	float xpos,ypos;		// x/y pos of target.
	float xpos2,ypos2;		// x/y pos of jump target.
	int jtime;			// time that a jump will occur (or negative if no jump)
	int vtype, vtype2;		// flag to indicate whether the target speed is fast or slow
	float vmin,vmax;		// velocity threshold for the original target
	float vmin2,vmax2;		// velocity threshold for the jump target
	int ibi;				// inter-beep interval (i.e., the TR interval)
	int iti;				//inter-trial interval
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
	//int drawtrace;                //trace number to draw
	int drawtext[5];              //write feedback text at the end of the block?
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

		int inputs_updated = 0;

		// Retrieve Flock of Birds data
		if (trackstatus>0)
		{
			// Update inputs from Flock of Birds
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
		//if (state == Finished)
			quit = true;

		// Get data from input devices
		if (inputs_updated > 0) // if there is a new frame of data
		{

			if (sensorsActive)
			{
				UCHAR bit = 4;  //this is the CTS line
				Target.PSstatus = PhotoSensor::GetSensorBitBang(ftHandle,bit);
			}
			else
				Target.PSstatus = -99;

			//updatedisplay = true;
			for (int a = ((trackstatus>0) ? 1 : 0); a <= ((trackstatus>0) ? BIRDCOUNT : 0); a++)
			{
				curs[a]->UpdatePos(dataframe.x[a],dataframe.y[a]);

				InputFrame i;
				i.time = dataframe.time[a];
				i.x = dataframe.x[a];
				i.y = dataframe.y[a];
				//i.z = dataframe.z[a];

				i.vel = curs[a]->GetVel();
				
				writer->Record(a, i, Target);
			}

		}

		game_update(); // Run the game loop (state machine update)

		//if (updatedisplay)  //reduce number of calls to draw_screen -- does this speed up display/update?
		draw_screen();

	}

	clean_up();
	return 0;
}


//function to read in the name of the trial table file, and then load that trial table
int LoadTrFile(char *fname)
{

	//std::cerr << "LoadTrFile begin." << std::endl;

	//char tmpline[50] = ""; 
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
		sscanf(tmpline, "%f %f %f %f %f %f %d %d %d %f %f %f %f %d %d", 
			&trtbl[ntrials].startx,&trtbl[ntrials].starty,
			&trtbl[ntrials].xpos,&trtbl[ntrials].ypos,
			&trtbl[ntrials].xpos2,&trtbl[ntrials].ypos2,
			&trtbl[ntrials].jtime,
			&trtbl[ntrials].vtype, &trtbl[ntrials].vtype2,
			&trtbl[ntrials].vmin,&trtbl[ntrials].vmax, 
			&trtbl[ntrials].vmin2,&trtbl[ntrials].vmax2,
			&trtbl[ntrials].ibi,
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
	//char dataPath[50] = DATA_OUTPUT_PATH;

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
		std::cerr << "Screen failed to build." << std::endl;
		return false;
	}
	else
		std::cerr << "Screen built." << std::endl;

	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0); //disable vsync

	setup_opengl();
	//Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 8, 4096);
	Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 512);
	if (TTF_Init() == -1)
	{
		std::cerr << "Audio failed to initialize." << std::endl;
		return false;
	}
	else
		std::cerr << "Audio initialized." << std::endl;

	//turn off the computer cursor
	SDL_ShowCursor(0);

	std::cerr << std::endl;

	// Load files and initialize pointers
	/*
	Image* tgttraces[NTRACES+1];  //is there a limit to the size of this array (stack limit?).  cannot seem to load more than 10 image traces...

	//load all the trace files
	for (a = 0; a < NTRACES; a++)
	{
		sprintf(tmpstr,"%s/Trace%d.png",TRACEPATH,a+1);
		tgttraces[a] = Image::LoadFromFile(tmpstr);
		if (tgttraces[a] == NULL)
			std::cerr << "Image Trace" << a << " did not load." << std::endl;
		else
		{
			targettraces[a] = new Object2D(tgttraces[a]);
			std::cerr << "   Trace " << a+1 << " loaded." << std::endl;
			//targettraces[a]->SetPos(PHYSICAL_WIDTH / 2, PHYSICAL_HEIGHT / 2);
		}
	}
	*/

	//load the velocity bar feedback frame
	VelBarFrame = Image::LoadFromFile("Resources/velbarframe.png");
	if (VelBarFrame == NULL)
		std::cerr << "Image VelBarFrame did not load." << std::endl;

	VelBarWin = Image::LoadFromFile("Resources/velbarwin.png");
	if (VelBarWin == NULL)
		std::cerr << "Image VelBarWin did not load." << std::endl;

	//load the velocity bar
	VelBar = Image::LoadFromFile("Resources/velbar.png");
	if (VelBar == NULL)
		std::cerr << "Image VelBar did not load." << std::endl;

	//std::cerr << "Images loaded: " << a-1 << "." << std::endl;


	startCircle = new Circle(curtr.startx, curtr.starty, START_RADIUS*2, startColor);
	startCircle->setBorderWidth(0.001f);
	startCircle->SetBorderColor(blkColor);
	startCircle->On();
	startCircle->BorderOn();

	targCircle = new Circle(curtr.startx+curtr.xpos, curtr.starty+curtr.ypos, TARGET_RADIUS*2, startColor);
	targCircle->SetBorderColor(blkColor);
	targCircle->setBorderWidth(0.002f);
	targCircle->BorderOn();
	targCircle->Off();
	
	photosensorCircle = new Circle(0.0f,0.24,0.05,blkColor);
	photosensorCircle->SetBorderColor(blkColor);
	photosensorCircle->BorderOff();
	photosensorCircle->On();
	

	//initialize the photosensor
	int status = -5;
	int devNum = 0;

	UCHAR Mask = 0x0f;  
	//the bits in the upper nibble should be set to 1 to be output lines and 0 to be input lines (only used 
	//  in SetSensorBitBang() ). The bits in the lower nibble should be set to 1 initially to be active lines.

	status = PhotoSensor::InitSensor(devNum,&ftHandle,1,Mask);
	std::cerr << "PhotoSensor: " << status << std::endl;

	PhotoSensor::SetSensorBitBang(ftHandle,Mask,3,0);

	UCHAR dataBit;

	//status = PhotoSensor::SetSensorBitBang(ftHandle,Mask,2,1);
	FT_GetBitMode(ftHandle, &dataBit);
	
	std::cerr << "DataByte: " << std::hex << dataBit << std::dec << std::endl;
	
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


	player->On();

	PeakVel = -1;


	//load sound files
	scorebeep = new Sound("Resources/correctbeep.wav");
	synchrobeep = new Sound("Resources/startbeep.wav");
	errorbeep = new Sound("Resources/errorbeep1.wav");


	/* To create text, call a render function from SDL_ttf and use it to create
	 * an Image object. See http://www.libsdl.org/projects/SDL_ttf/docs/SDL_ttf.html#SEC42
	 * for a list of render functions.
	 */
	font = TTF_OpenFont("Resources/arial.ttf", 28);

	text = new Image(TTF_RenderText_Blended(font, " ", textColor));
	
	std::stringstream textstring;		
	textstring << "Too Late ";
	textTooLate = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	textstring.str("");
	textstring << "Too Early ";
	textTooEarly = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));
	
	textstring.str("");
	textstring << "Wrong Speed!";
	textFailVelCrit = new Image(TTF_RenderText_Blended(font, textstring.str().c_str(), textColor));

	trialnumfont = TTF_OpenFont("Resources/arial.ttf", 12);
	trialnum = new Image(TTF_RenderText_Blended(trialnumfont, "1", textColor));


	SDL_WM_SetCaption("TR Target Jump", NULL);

	
	// Set the initial game state
	state = Idle; 

	std::cerr << "initialization complete." << std::endl;
	return true;
}


static void setup_opengl()
{
	glClearColor(1, 1, 1, 0);

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

}


//end the program; clean up everything neatly.
void clean_up()
{
	delete startCircle;
	delete targCircle;
	delete scorebeep;
	delete synchrobeep;
	delete errorbeep;
	
	/*
	for (int a = 0; a < NTRACES; a++)
		delete targettraces[a];
	*/

	int status = PhotoSensor::CloseSensor(ftHandle,1);

	delete text;
	delete textTooEarly;
	delete textTooLate;
	delete textFailVelCrit;

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
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//barrierset[1].Draw();

	/*
	//draw the trace specified
	if (drawstruc.drawtrace >= 0)
	{
		//targettraces[drawstruc.drawtrace]->Draw(PHYSICAL_WIDTH, PHYSICAL_HEIGHT);
		targettraces[drawstruc.drawtrace]->Draw();
		//barrierset[drawstruc.drawtrace].Draw();
		Target.trace = drawstruc.drawtrace+1;
	}
	else
		Target.trace = -1;
	*/

	//draw the velocity feedback bar, unless we have reached the end of the block (write text instead)
	if ((drawstruc.drawvelbar >= 0) && (drawstruc.drawtext[0]==0))
	{
		VelBarFrame->DrawAlign(22*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16,VelBarFrame->GetWidth(),VelBarFrame->GetHeight(),4);
		if (VminShow > 0 && VmaxShow > 0)
			VelBarWin->DrawAlign(22*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16+(VelBarFrame->GetHeight()/(2.0f-0.0f))*VminShow,VelBarWin->GetWidth(),(VelBarFrame->GetHeight()/(2.0f-0.0f))*(VmaxShow-VminShow),4);
		VelBar->DrawAlign(22*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16,VelBarFrame->GetWidth()*0.8,VelBarFrame->GetHeight()*drawstruc.drawvelbar,4);	
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


	// Draw text - provide feedback at the end of the block
	if (drawstruc.drawtext[0] == 1)
	{
		//provide the score at the end of the block.
		std::stringstream scorestring;
			scorestring << "You earned " 
				        << score 
						<< " points.";
			text = new Image(TTF_RenderText_Blended(font, scorestring.str().c_str(), textColor));
			text->Draw(0.6f, 0.5f);
			//for (int a=1; a<5; a++)
			//	drawstruc.drawtext[a] = 0;
	}
	
	// Draw Timing Feedback text
	if (drawstruc.drawtext[1] == 1)
	{
		textTooLate->Draw(0.6f, 0.44f);
	}
	else if (drawstruc.drawtext[1] == -1)
	{
		textTooEarly->Draw(0.6f, 0.44f);
	}

	if (drawstruc.drawtext[2] == 1)
	{
		textFailVelCrit->Draw(0.6f,0.47f);
	}

	//write the trial number
	trialnum->Draw(PHYSICAL_WIDTH*23/24, PHYSICAL_HEIGHT*23/24);

	SDL_GL_SwapBuffers();
	glFlush();

}

//game update loop - state machine controlling the status of the experiment
Uint32 TstartTrial = 0;

bool timingSuccess = false;

bool mvtStarted = false;
Uint32 timeMvtStart = TstartTrial;

bool reachedvelmin = false;
bool reachedvelmax = false;

bool targetJumpFlag = false;

bool mvmtEnded = false;
bool hitTarget = false;

float xp,yp;
int ds;

float LastPeakVel = 0;
bool returntostart = true;

void game_update()
{

	switch (state)
	{
		case Idle:
			/* If player starts hovering over start marker, set state to Starting
			 * and store the time -- this state (Idle) is for trial #1 only.
			 */

			Target.trial = 0;

			//drawstruc.drawtrace = -1; //for normal operations, show no trace
			//drawstruc.drawtrace = 0;  //for calibration purposes, display trace0 always
			startCircle->SetPos(curtr.startx, curtr.starty);
			startCircle->On();
			targCircle->Off();
			for (int a = 0; a < 5; a++)
				drawstruc.drawtext[a] = 0;
			drawstruc.drawvelbar = -1;

			photosensorCircle->On();

			trtbl[CurTrial];
			xp = startCircle->GetX();
			yp = startCircle->GetY();
			ds = startCircle->drawState();

			if (!returntostart && CurTrial > 0)
			{
				//if we haven't yet gotten back to the start target, show the old trace and the old velocity feedback
				//drawstruc.drawtrace = -1; //curTrial counts from 1, but indexes an array indexed from 0.  I think traces does the same...
				startCircle->On();
				targCircle->Off();
				drawstruc.drawvelbar = (LastPeakVel-0.0f)/(2.0f-0.0f);  //draw the velocity feedback bar
				drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0.0f : drawstruc.drawvelbar);
				drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1.0f : drawstruc.drawvelbar);				
				VminShow = trtbl[CurTrial-1].vmin;
				VmaxShow = trtbl[CurTrial-1].vmax;
			}

			if( (player->Distance(startCircle) <= CURSOR_RADIUS*1.5) && (CurTrial < NTRIALS) )
			{
				//targettraces[curtr.trace]->SetPos(curtr.startx, curtr.starty);
				//drawstruc.drawtrace = -1;
				hoverTimer = SDL_GetTicks();
				gameTimer = SDL_GetTicks();
				
				Target.jumped = 0;
				Target.lat = -999;

				std::cerr << "Leaving IDLE state." << std::endl;
				
				//Target.trial = CurTrial;

				LastPeakVel = PeakVel;
				PeakVel = 0;

				currToneRingNum = 0;
				Target.beep = currToneRingNum;

				returntostart = true;
				state = Starting;
				//updatedisplay = true;
			}
			break;
		case Starting: 
			/* If player stops hovering over start marker, set state to Idle and
			 * store the time  -- this is for new trials only!
			 */
			//drawstruc.drawtrace = -1;

			//drawstruc.drawtrace = -1; // for TRbase, no traces are needed
			startCircle->On();
			startCircle->SetColor(startColor);
			targCircle->Off();
			drawstruc.drawvelbar = (PeakVel-0.0f)/(2.0f-0.0f);  //draw the velocity feedback bar
			drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0.0f : drawstruc.drawvelbar);
			drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1.0f : drawstruc.drawvelbar);
			//VminShow = curtr.vmin;
			//VmaxShow = curtr.vmax;
			VminShow = 0;
			VmaxShow = 0;


			for (int a = 0; a < 5; a++)
				drawstruc.drawtext[a] = 0;
			
			if (player->Distance(startCircle) > START_RADIUS)
			{
				state = Idle;
			}
			// If player hovers long enough, set state to Active
			else if (SDL_GetTicks() - hoverTimer >= curtr.iti)
			{
				Target.trial = CurTrial+1;

				targCircle->SetPos(curtr.startx+curtr.xpos, curtr.starty+curtr.ypos);
				if (curtr.vtype == 2)
					targCircle->SetColor(targetFastColor);
				else if (curtr.vtype == 1)
					targCircle->SetColor(targetSlowColor);
				else
					targCircle->SetColor(startColor);
				targCircle->SetBorderColor(blkColor);
				targCircle->On();
				photosensorCircle->Off();

				synchrobeep->Play();
				gameTimer = SDL_GetTicks();
				TstartTrial = gameTimer;
				lastToneRing = gameTimer;

				reachedvelmin = false;
				reachedvelmax = false;
				timingSuccess = false;
				mvtStarted = false;
				targetJumpFlag = false;
				timeMvtStart = gameTimer;
				currToneRingNum = 1;
				PeakVel = 0;
				Target.beep = currToneRingNum;

				mvmtEnded = false;
				hitTarget = false;

				Target.vmin = curtr.vmin;
				Target.vmax = curtr.vmax;
				Target.jtime = curtr.jtime;

				std::cerr << "Leaving STARTING state." << std::endl;
				state = Active;
			}

			break;

		case Active:
			
			//drawstruc.drawtrace = -1;
			startCircle->On();
			targCircle->On();

			//ring the synchro bell once at each of 0, 500, 1000, and 1500:
			if (currToneRingNum < MAX_TONE_RINGS)
			{
				if ((SDL_GetTicks() - lastToneRing) >= curtr.ibi)
				{
					synchrobeep->Play();
					//lastToneRing = currTime;
					lastToneRing = SDL_GetTicks();
					currToneRingNum++;
					Target.beep = currToneRingNum;
				}
			}

			//check if a jump in target is to occur.  if so, arrange timing:
			if(curtr.jtime > 0)
			{
				if( (SDL_GetTicks() - TstartTrial >= curtr.jtime+curtr.ibi) && !targetJumpFlag)
				{
					targCircle->SetPos(curtr.startx+curtr.xpos2, curtr.starty+curtr.ypos2);
					
					curtr.vmin = curtr.vmin2;  //we will overwrite these values with the updated values, rather than having to keep track of whether we jumped or not
					curtr.vmax = curtr.vmax2;
					Target.vmin = curtr.vmin2;
					Target.vmax = curtr.vmax2;

					curtr.vtype = curtr.vtype2;

					if (curtr.vtype2 == 2)
						targCircle->SetColor(targetFastColor);
					else if (curtr.vtype2 == 1)
						targCircle->SetColor(targetSlowColor);
					else
						targCircle->SetColor(startColor);

					photosensorCircle->On();
					Target.jumped = 1;

					//VminShow = curtr.vmin2;
					//VmaxShow = curtr.vmax2;
					VminShow = 0;
					VmaxShow = 0;

					targetJumpFlag = true;
				}
			}


			//detect the onset of hand movement, for calculating latency
			if (!mvtStarted && (player->Distance(startCircle) > START_RADIUS*1.5))
			{
				timeMvtStart = SDL_GetTicks();
				mvtStarted = true;
				Target.lat = timeMvtStart - gameTimer;
			}


			//keep track of the maximum (peak) velocity -- this will be plotted in the feedback bar
			if ((player->GetVel() > PeakVel) && (player->Distance(startCircle) <= targCircle->Distance(startCircle)) )
				PeakVel = player->GetVel();

			//***
			//note, a potential problem here if the movement begins BEFORE the target jump: a flag could be set before the velocity criteria are updated!!!
			//***

			//note if the velocity exceeded the minimum required velocity
			if (player->GetVel() > curtr.vmin)
			{
				//std::cerr << "Reached VelMin." << std::endl;
				reachedvelmin = true;
			}
			//note if the velocity exceeded the maximum  required velocity
			if (player->GetVel() > curtr.vmax)
			{
				//std::cerr << "Reached VelMax." << std::endl;
				reachedvelmax = true;
			}

			
			//check if the player hit the target
			if (player->HitTarget(targCircle))
			{
				hitTarget = true;
				targCircle->SetColor(targHitColor);
			}


			//detect movement offset
			if (!mvmtEnded && mvtStarted && (player->GetVel() < VEL_MVT_TH) && (timeMvtStart-SDL_GetTicks())>200 && player->Distance(startCircle)>4*START_RADIUS)
				{
					mvmtEnded = true;
					hoverTimer = SDL_GetTicks();
					std::cerr << "Mvmt Ended: " << float(SDL_GetTicks()) << std::endl;
				}

			if ((player->GetVel() >= VEL_MVT_TH))
			{
				mvmtEnded = false;
				hoverTimer = SDL_GetTicks();
			}


			//if the trial duration is exceeded, the hand has stopped moving, or the hand has exceeded the target array, end the trial
			if ( ((SDL_GetTicks() - lastToneRing) > MAX_TRIAL_DURATION) || (mvmtEnded && (SDL_GetTicks()-hoverTimer)>VEL_END_TIME ) || (player->Distance(startCircle) > targCircle->Distance(startCircle) ) )
			{
				
				LastPeakVel = PeakVel;
				PeakVel = 0;

				photosensorCircle->On();

				Target.lat = int(timeMvtStart)-int(lastToneRing);
				if (Target.lat < -5*curtr.ibi)
					Target.lat = 5*curtr.ibi;
				if ( (Target.lat) > TIMING_TOL )
				{
					timingSuccess = false;
					drawstruc.drawtext[1] = 1;
					startCircle->SetColor(redColor);
				}
				else if ( (Target.lat) < -TIMING_TOL )
				{
					timingSuccess = false;
					drawstruc.drawtext[1] = -1;
					startCircle->SetColor(greenColor);
				}
				else
				{
					timingSuccess = true;
					drawstruc.drawtext[1] = 0;
					startCircle->SetColor(whiteColor);
				}

				//std::cerr << "Target scored."  << std::endl;
				std::cerr << "Score Flags: " << (reachedvelmin ? "1" : "0")
					 	  << (reachedvelmax ? "1" : "0")
						  << (hitTarget ? "1" : "0")
						  << (timingSuccess ? "1" : "0")
						  << std::endl;

				if (reachedvelmin && !reachedvelmax && hitTarget && timingSuccess )  //if the velocity and latency criteria have been satisified and the target has been hit, score the trial
				{
					score++;     //target score
					scorebeep->Play();
				}
				else
				{
					if (!timingSuccess)  //if wrong latency
					{
						errorbeep->Play();  //bad latency!
					}
					if (hitTarget)  //if hit the target
					{
						//do nothing
					}
					if (!reachedvelmin || reachedvelmax)
					{
						drawstruc.drawtext[2] = 1;

						//note whether the movement during the trial satisified the velocity requirements
						if (!reachedvelmin)
							std::cerr << "Minimum Velocity not met."  << std::endl;
						if (reachedvelmax)
							std::cerr << "Maximum Velocity exceeded."  << std::endl;

						if (!hitTarget)
							targCircle->SetColor(blkColor);

						if (curtr.vtype == 2)
							targCircle->SetBorderColor(targetFastColor);
						else if (curtr.vtype == 1)
							targCircle->SetBorderColor(targetSlowColor);
						else
							targCircle->SetBorderColor(startColor);
					}

				}

				//go to ShowResult state
				gameTimer = SDL_GetTicks();
				state = ShowResult;

			}
			
			break;

		case ShowResult:

			returntostart = false;

			drawstruc.drawvelbar = (LastPeakVel-0.0f)/(2.0f-0.0f);  //draw the velocity feedback bar
			drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0 : drawstruc.drawvelbar);
			drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1 : drawstruc.drawvelbar);
			VminShow = curtr.vmin;
			VmaxShow = curtr.vmax;

			if ( (SDL_GetTicks() - gameTimer) > HOLDTIME)
			{

				CurTrial++;
				std::stringstream texttn;
				texttn << CurTrial+1;  //CurTrial starts from 0, so we add 1 for convention.
				delete trialnum;
				trialnum = new Image(TTF_RenderText_Blended(trialnumfont, texttn.str().c_str(), textColor));
				std::cerr << "Trial " << CurTrial << " ended at " << SDL_GetTicks() 
					<< ". Elapsed time, " << (SDL_GetTicks() - TstartTrial) << std::endl;


				//if we have reached the end of the trial table, quit
				if (CurTrial >= NTRIALS)
				{
					std::cerr << "Leaving ACTIVE state to FINISHED state." << std::endl;
					gameTimer = SDL_GetTicks();
					state = Finished;
				}
				else
				{
					hoverTimer = SDL_GetTicks();
					std::cerr << "Leaving ACTIVE state to Idle state." << std::endl;
					state = Idle;
				}
			}

			break;

		case Finished:
			// Trial table ended, wait for program to quit

			startCircle->Off();
			targCircle->Off();

			//drawstruc.drawtrace = -1;
			
			drawstruc.drawtext[0] = 1;
			drawstruc.drawtext[1] = 0;
			drawstruc.drawtext[2] = 0;

			break;

	}
}

