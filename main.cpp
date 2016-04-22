#include <cmath>
#include <sstream>
#include <iostream>
#include <fstream>
#include <istream>
#include <windows.h>
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include "FTDI.h"

#include "MouseInput.h"
#include "TrackBird.h"

#include "Circle.h"
#include "DataWriter.h"
#include "HandCursor.h"
#include "Object2D.h"
#include "Path2D.h"
#include "Region2D.h"
#include "Sound.h"

#include "config.h"

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
#pragma comment(lib, "ftd2xx.lib")
#pragma comment(lib, "ATC3DG.lib")
*/
#pragma push(1)

//state machine
enum GameState
{
	Idle = 0x01,       //00001
	Starting = 0x03,   //00011
	Active = 0x06,     //00110
	ShowResult = 0x08, //01000
	Finished = 0x10    //10000
};



SDL_Event event;
SDL_Surface* screen = NULL;

Circle* cursor[BIRDCOUNT + 1];
HandCursor* curs[BIRDCOUNT + 1];
HandCursor* player = NULL;
Circle* startCircle = NULL;
Circle* targCircle = NULL;
Circle* photosensorCircle = NULL;
//Object2D* targettraces[NTRACES];
//float barrierColor[3] = {.5, .5, .5};
Region2D barrierRegions[NREGIONS];
Path2D barrierPaths[NPATHS];
Object2D* traces[NTRACES];
Image* text = NULL;
Image* trialnum = NULL;
Sound* scorebeep = NULL;
Sound* errorbeep = NULL;
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


//Photosensor variables
FT_HANDLE ftHandle;
bool sensorsActive;

//tracker variables
int trackstatus;
TrackSYSCONFIG sysconfig;
TrackDATAFRAME dataframe[BIRDCOUNT+1];
Uint32 DataStartTime = 0;

//variables to compute the earned score
int score = 0;

//colors
float redColor[3] = {1.0f, 0.0f, 0.0f};
float greenColor[3] = {0.0f, 1.0f, 0.0f};
float blueColor[3] = {0.0f, 0.0f, 1.0f};
float cyanColor[3] = {0.0f, 0.5f, 1.0f};
float grayColor[3] = {0.6f, 0.6f, 0.6f};
float blkColor[3] = {0.0f, 0.0f, 0.0f};
float whiteColor[3] = {1.0f, 1.0f, 1.0f};
float orangeColor[3] = {1.0f, 0.5f, 0.0f};
float *startColor = greenColor;
float *targColor = blueColor;
float *targHitColor = cyanColor;
float *cursColor = redColor;



// Trial table structure, to keep track of all the parameters for each trial of the experiment
typedef struct {
	//int TrialType;		// Flag 1-for trial type
	float startx,starty;	// x/y pos of start target; also, trace will be centered here!
	float xpos,ypos;		// x/y pos of target.
	int path;
	int region;
	int trace;
	int iti;				//inter-trial interval
} TRTBL;

#define TRTBL_SIZE 1000
TRTBL trtbl [TRTBL_SIZE];

int NTRIALS = 0;
int CurTrial = 0;

#define curtr trtbl[CurTrial]

//target structure; keep track of the target and other parameters, for writing out to data stream
TargetFrame Target;

//structure to keep track of what to draw in the draw_screen() function
typedef struct {
	int drawtrace;                //trace number to draw
	int drawtext[5];              //write feedback text at the end of the block?
	int drawpath;
	int drawregion;
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
		int inputs_updated = 0;

		// Retrieve Flock of Birds data
		if (trackstatus>0)
		{
			// Update inputs from Flock of Birds
			inputs_updated = TrackBird::GetUpdatedSample(&sysconfig,dataframe);
		}

		// Handle SDL events
		while (SDL_PollEvent(&event))
		{
			// See http://www.libsdl.org/docs/html/sdlevent.html for list of event types
			if (event.type == SDL_MOUSEMOTION)
			{
				if (trackstatus <= 0)
				{
					MouseInput::ProcessEvent(event);
					inputs_updated = MouseInput::GetFrame(dataframe);

				}
			}
			else if (event.type == SDL_KEYDOWN)
			{
				// See http://www.libsdl.org/docs/html/sdlkey.html for Keysym definitions
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					quit = true;
				}
				else //if( event.key.keysym.unicode < 0x80 && event.key.keysym.unicode > 0 )
				{
					Target.key = *SDL_GetKeyName(event.key.keysym.sym);  //(char)event.key.keysym.unicode;
					//std::cerr << Target.flag << std::endl;
				}
			}
			else if (event.type == SDL_KEYUP)
			{
				Target.key = '0';
			}
			else if (event.type == SDL_QUIT)
			{
				quit = true;
			}
		}

		if ((CurTrial >= NTRIALS) && (state == Finished) && (SDL_GetTicks() - gameTimer >= 10000))
			quit = true;

		// Get data from input devices
		if (inputs_updated > 0) // if there is a new frame of data
		{

			if (sensorsActive)
			{
				UCHAR bit = 4;  //this is the CTS line
				Target.PSstatus = Ftdi::GetFtdiBitBang(ftHandle,bit);
			}
			else
				Target.PSstatus = -99;

			//updatedisplay = true;
			for (int a = ((trackstatus>0) ? 1 : 0); a <= ((trackstatus>0) ? BIRDCOUNT : 0); a++)
			{
				if (dataframe[a].ValidInput)
				{
					curs[a]->UpdatePos(dataframe[a].x,dataframe[a].y);
					dataframe[a].vel = curs[a]->GetVel();
					writer->Record(a, dataframe[a], Target);
				}
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

	char tmpline[100] = ""; 
	int ntrials = 0;

	//read in the trial file name
	std::ifstream trfile(fname);

	if (!trfile)
	{
		std::cerr << "Cannot open input file." << std::endl;
		return(-1);
	}
	else
		std::cerr << "Opened TrialFile " << TRIALFILE << std::endl;

	trfile.getline(tmpline,sizeof(tmpline),'\n');  //get the first line of the file, which is the name of the trial-table file

	while(!trfile.eof())
	{
		sscanf(tmpline, "%f %f %f %f %d %d %d %d", 
			&trtbl[ntrials].startx,&trtbl[ntrials].starty,
			&trtbl[ntrials].xpos,&trtbl[ntrials].ypos,
			&trtbl[ntrials].path,
			&trtbl[ntrials].region,
			&trtbl[ntrials].trace, 
			&trtbl[ntrials].iti);

			ntrials++;
			trfile.getline(tmpline,sizeof(tmpline),'\n');
	}

	trfile.close();
	if(ntrials == 0)
	{
		std::cerr << "Empty input file." << std::endl;
		//exit(1);
		return(-1);
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
	
	Image* tgttraces[NTRACES];

	//load all the trace files
	for (a = 0; a < NTRACES; a++)
	{
		sprintf(tmpstr,"%s/Trace%d.png",TRACEPATH,a);
		tgttraces[a] = Image::LoadFromFile(tmpstr);
		if (tgttraces[a] == NULL)
			std::cerr << "Image Trace" << a << " did not load." << std::endl;
		else
		{
			traces[a] = new Object2D(tgttraces[a]);
			std::cerr << "   Trace " << a << " loaded." << std::endl;
			traces[a]->SetPos(PHYSICAL_WIDTH / 2, PHYSICAL_HEIGHT / 2);
		}
	}
	

	//load the path files
	for (a = 0; a < NPATHS; a++)
	{
		sprintf(tmpstr,"%s/Path%d.txt",PATHPATH,a); 
		barrierPaths[a] = Path2D::LoadPathFromFile(tmpstr);
		if (barrierPaths[a].GetPathNVerts() < 0)
			std::cerr << "   Path " << a << " did not load." << std::endl;
		else
			std::cerr << "   Path " << a << " loaded." << std::endl;
	}

	//load the region files
	for (a = 0; a < NREGIONS; a++)
	{
		sprintf(tmpstr,"%s/Region%d.txt",REGIONPATH,a); 
		barrierRegions[a] = Region2D::LoadPolyFromFile(tmpstr);
		if (barrierRegions[a].GetPolySides() <= 2)
			std::cerr << "   Region " << a << " did not load." << std::endl;
		else
			std::cerr << "   Region " << a << " loaded." << std::endl;
	}


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

	status = Ftdi::InitFtdi(devNum,&ftHandle,1,Mask);
	std::cerr << "PhotoSensor: " << status << std::endl;

	Ftdi::SetFtdiBitBang(ftHandle,Mask,3,0);

	UCHAR dataBit;

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
	errorbeep = new Sound("Resources/errorbeep1.wav");


	/* To create text, call a render function from SDL_ttf and use it to create
	 * an Image object. See http://www.libsdl.org/projects/SDL_ttf/docs/SDL_ttf.html#SEC42
	 * for a list of render functions.
	 */
	font = TTF_OpenFont("Resources/arial.ttf", 28);

	text = new Image(TTF_RenderText_Blended(font, " ", textColor));
	
	trialnumfont = TTF_OpenFont("Resources/arial.ttf", 12);
	trialnum = new Image(TTF_RenderText_Blended(trialnumfont, "1", textColor));


	SDL_WM_SetCaption("Compiled Code", NULL);

	
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
	delete errorbeep;
	
	
	for (int a = 0; a < NTRACES; a++)
		delete traces[a];
	

	int status = Ftdi::CloseFtdi(ftHandle,1);

	delete text;

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

	
	//draw the trace specified
	if (drawstruc.drawtrace >= 0)
	{
		traces[drawstruc.drawtrace]->SetPos(curtr.startx,curtr.starty);
		traces[drawstruc.drawtrace]->Draw();
		Target.trace = drawstruc.drawtrace;
	}
	else
		Target.trace = -1;
	

	//draw the velocity feedback bar, unless we have reached the end of the block (write text instead)
	if ((drawstruc.drawvelbar >= 0) && (drawstruc.drawtext[0]==0))
	{
		VelBarFrame->DrawAlign(25*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16,VelBarFrame->GetWidth(),VelBarFrame->GetHeight(),4);
		VelBarWin->DrawAlign(25*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16+(VelBarFrame->GetHeight()/(VELBARMAX-VELBARMIN))*VELMIN,VelBarWin->GetWidth(),(VelBarFrame->GetHeight()/(VELBARMAX-VELBARMIN))*(VELMAX-VELMIN),4);
		VelBar->DrawAlign(25*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16,VelBarFrame->GetWidth()*0.8,VelBarFrame->GetHeight()*drawstruc.drawvelbar,4);	
	}
	
	//draw the region
	if (drawstruc.drawregion >= 0)
	{
		barrierRegions[drawstruc.drawregion].Draw(curtr.startx,curtr.starty);
		Target.path = drawstruc.drawregion;
	}
	else
		Target.path = -1;

	//draw the path
	if (drawstruc.drawpath >= 0)
	{
		barrierPaths[drawstruc.drawpath].Draw(curtr.startx+curtr.xpos,curtr.starty+curtr.ypos);
		Target.path = drawstruc.drawpath;
	}
	else
		Target.path = -1;


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
	

	//write the trial number
	trialnum->Draw(PHYSICAL_WIDTH*23/24, PHYSICAL_HEIGHT*23/24);

	SDL_GL_SwapBuffers();
	glFlush();

}


//game update loop - state machine controlling the status of the experiment
bool mvtStarted = false;
Uint32 timeMvtStart;

bool reachedvelmin = false;
bool reachedvelmax = false;

bool mvmtEnded = false;
bool hitTarget = false;

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
			startCircle->SetPos(curtr.startx, curtr.starty);
			startCircle->On();
			targCircle->Off();
			for (int a = 0; a < 5; a++)
				drawstruc.drawtext[a] = 0;
			drawstruc.drawvelbar = -1;

			photosensorCircle->On();

			if (!returntostart && CurTrial > 0)
			{
				//if we haven't yet gotten back to the start target yet
				startCircle->On();
				targCircle->Off();
				drawstruc.drawvelbar = (LastPeakVel-VELBARMIN)/(2*(VELBARMAX-VELBARMIN));  //draw the velocity feedback bar; the valid region is the lower half of the bar.
				drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0 : drawstruc.drawvelbar);
				drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1 : drawstruc.drawvelbar);				
			}

			drawstruc.drawpath = -1;
			drawstruc.drawregion = -1;
			for (int a = 0; a < NTRACES; a++)
				traces[a]->Off();
			
			if (curtr.trace >= 0)
			{
				traces[curtr.trace]->On();
				drawstruc.drawtrace = curtr.trace;
			}

			for (int a = 0; a < NPATHS; a++)
				barrierPaths[a].SetPathColor(grayColor);

			for (int a = 0; a < NREGIONS; a++)
				barrierRegions[a].SetPolyColor(redColor);

			

			if( (player->Distance(startCircle) <= CURSOR_RADIUS*1.5) && (CurTrial < NTRIALS) )
			{
				hoverTimer = SDL_GetTicks();
				gameTimer = SDL_GetTicks();
				
				std::cerr << "Leaving IDLE state." << std::endl;
				
				LastPeakVel = PeakVel;
				PeakVel = 0;

				returntostart = true;
				state = Starting;
			}
			break;
		case Starting: 
			/* If player stops hovering over start marker, set state to Idle and
			 * store the time  -- this is for new trials only!
			 */

			//drawstruc.drawtrace = -1; // for TRbase, no traces are needed
			startCircle->On();
			startCircle->SetColor(startColor);
			targCircle->Off();
			drawstruc.drawvelbar = (PeakVel-VELBARMIN)/(2*(VELBARMAX-VELBARMIN));  //draw the velocity feedback bar; the valid region is the lower half of the bar.
			drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0 : drawstruc.drawvelbar);
			drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1 : drawstruc.drawvelbar);				

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
				targCircle->SetColor(targColor);
				targCircle->SetBorderColor(blkColor);
				targCircle->On();
				photosensorCircle->Off();

				drawstruc.drawtrace = curtr.trace;
				drawstruc.drawpath = curtr.path;
				drawstruc.drawregion = curtr.region;

				gameTimer = SDL_GetTicks();

				reachedvelmin = false;
				reachedvelmax = false;
				mvtStarted = false;
				PeakVel = 0;

				mvmtEnded = false;
				hitTarget = false;

				std::cerr << "Leaving STARTING state." << std::endl;
				state = Active;
			}

			break;

		case Active:
			
			//drawstruc.drawtrace = -1;
			startCircle->On();
			targCircle->On();

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

			//note if the velocity exceeded the minimum required velocity
			if (player->GetVel() > VELMIN)
			{
				//std::cerr << "Reached VelMin." << std::endl;
				reachedvelmin = true;
			}
			//note if the velocity exceeded the maximum  required velocity
			if (player->GetVel() > VELMAX)
			{
				//std::cerr << "Reached VelMax." << std::endl;
				reachedvelmax = true;
			}

			if (barrierPaths[drawstruc.drawpath].OnPath(player,curtr.startx+curtr.xpos,curtr.starty+curtr.ypos))
				barrierPaths[drawstruc.drawpath].SetPathColor(orangeColor);

			if (barrierRegions[drawstruc.drawpath].InRegion(player,curtr.startx,curtr.starty))
				barrierRegions[drawstruc.drawpath].SetPolyColor(orangeColor);


			
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
			if ( ((SDL_GetTicks() - gameTimer) > MAX_TRIAL_DURATION) || (mvmtEnded && (SDL_GetTicks()-hoverTimer)>VEL_END_TIME ) || (player->Distance(startCircle) > targCircle->Distance(startCircle) ) )
			{
				LastPeakVel = PeakVel;
				PeakVel = 0;

				photosensorCircle->On();

				//std::cerr << "Target scored."  << std::endl;
				std::cerr << "Score Flags: " << (reachedvelmin ? "1" : "0")
					 	  << (reachedvelmax ? "1" : "0")
						  << (hitTarget ? "1" : "0")
						  << std::endl;

				if (reachedvelmin && !reachedvelmax && hitTarget)  //if the velocity and latency criteria have been satisified and the target has been hit, score the trial
				{
					score++;     //target score
					scorebeep->Play();
				}
				else
				{
					if (hitTarget)  //if hit the target
					{
						//do nothing
					}
					if (!reachedvelmin || reachedvelmax)
					{
						errorbeep->Play();

						//note whether the movement during the trial satisified the velocity requirements
						if (!reachedvelmin)
							std::cerr << "Minimum Velocity not met."  << std::endl;
						if (reachedvelmax)
							std::cerr << "Maximum Velocity exceeded."  << std::endl;

						if (!hitTarget)
							targCircle->SetColor(blkColor);

					}

				}

				//go to ShowResult state
				gameTimer = SDL_GetTicks();
				state = ShowResult;

			}
			
			break;

		case ShowResult:

			returntostart = false;

			drawstruc.drawvelbar = (LastPeakVel-VELBARMIN)/(2*(VELBARMAX-VELBARMIN));  //draw the velocity feedback bar; the valid region is the lower half of the bar.
			drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0 : drawstruc.drawvelbar);
			drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1 : drawstruc.drawvelbar);				

			if ( (SDL_GetTicks() - gameTimer) > HOLDTIME)
			{

				CurTrial++;
				std::stringstream texttn;
				texttn << CurTrial+1;  //CurTrial starts from 0, so we add 1 for convention.
				delete trialnum;
				trialnum = new Image(TTF_RenderText_Blended(trialnumfont, texttn.str().c_str(), textColor));
				std::cerr << "Trial " << CurTrial << " ended at " << SDL_GetTicks() << std::endl;

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

			drawstruc.drawpath = -1;
			drawstruc.drawregion = -1;
			drawstruc.drawtrace = -1;

			//drawstruc.drawtrace = -1;
			
			drawstruc.drawtext[0] = 1;

			break;

	}
}

