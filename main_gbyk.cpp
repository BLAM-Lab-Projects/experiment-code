/* Control software for kinereach
See readme.txt for further details

Authors:
Adrian Haith
Promit Roy
Aneesh Roy
Aaron Wong

This code was based on the SMARTS2 software package written by Aaron Wong, last modified 8/6/2014

This code runs the go-before-you-know task according to a trial table.

*/

#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <windows.h>
#include <direct.h>
#include <string>
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#include "bird.h"
#include "DataWriter.h"
#include "TrackBird.h"
#include "Object2D.h"
#include "Sound.h"
#include "Circle.h"
#include "HandCursor.h"
#include "TargetFrame.h"
#include "Path2D.h"

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

enum GameState // defines possible states to control the flow of each trial
{
	Idle = 0x01,     //00001
	WaitStart = 0x03, //00011
	ShowTgts = 0x08,   //00100
	GoTgts = 0x09,
	Finished = 0x0A,  //10000
	Exiting = 0x0F,
};

SDL_Event event;
SDL_Surface* screen = NULL;
bool quit = false;
Circle* cursors[BIRDCOUNT + 1];
HandCursor* curs[BIRDCOUNT + 1];
HandCursor* player = NULL;
//Object2D* targettraces[NTRACES+1];
//GLfloat failColor[3] = {1.0f, 0.0f, 0.0f};
Image* trialnum = NULL;
Image* scoretext = NULL;
Image* trscoretext = NULL;
Image* latestarttxt = NULL;
Image* velfasttxt = NULL;
Image* velslowtxt = NULL;
Sound* hit = NULL;
Sound* startbeep = NULL;
Sound* errorbeep = NULL;
Sound* errorbeep2 = NULL;
TTF_Font* font = NULL;
TTF_Font* trialnumfont = NULL;
SDL_Color textColor = {0, 0, 0};
DataWriter* writer = NULL;
GameState state;
Uint32 gameTimer;
Uint32 hoverTimer;
Uint32 hitTimer;
int trackstatus;

// Trial table structure, to keep track of all the parameters for each trial of the experiment
typedef struct {
	int TrialType;			// Flag for type of trial: 1 = 1 tgt, 2 = 2 tgt
	float startx,starty;	// x/y pos of start target; also, trace will be centered here!  
	float xpos,ypos;		// x/y pos of the target.
	float x2pos,y2pos;		// x/y pos of the second target (if it is a two-target trial).
	//int trace;				//trace (>= 0) for cuing how to draw the path; if value is <0 no trace will be displayed.  Traces should be numbered sequentially starting from 0.
	int holdtime;			//time to wait before the targets appear
	int tdeadline;			//time that the movement must be launched by
	float velmin;			//minimum velocity for this reach
	float velmax;			//maximum velocity for this reach
} TRTBL;

#define TRTBL_SIZE 1000
TRTBL trtbl [TRTBL_SIZE];

int NTRIALS = 0;
int CurTrial = 0;

#define curtr trtbl[CurTrial]


//structure to keep track of what to draw in the draw_screen() function
typedef struct {
	//int drawtrace;				  //trace number to draw (for intro screen)
	int drawtext[5];              //write feedback text, depending on what flags are set
	float drawvelbar;             //velocity feedback-bar parameter
} DRAWSTRUC;

DRAWSTRUC drawstruc;

float PeakVel;  //constant to keep track of the peak velocity in the trial

Circle* targCircle1;
Circle* targCircle2;
Circle* cursCircle;
Circle* startCircle;

//float targetColor[3] = {0.6f, 0.6f, 0.6f};
float hitColor[3] = {0.f, 1.f, .0f};
float cursColor[3] = {0.0f, 0.0f, 1.0f};
float startColor[3] = {.6f,.6f,.6f};
float targetColor[3] = {0.0f, 0.5f, 1.0f};
float blkColor[3] = {0.0f, 0.0f, 0.0f};
float whiteColor[3] = {1.0f, 1.0f, 1.0f};
float grayColor[3] = {0.5f, 0.5f, 0.5f};

int trialDone = 0;

//target structure; keep track of where the current target is now (only 1!)
TargetFrame Target;

//velocity-bar images
Image* VelBarFrame;
Image* VelBarWin;
Image* VelBar;

//variables to compute the earned score
int score = 0;
int trscore = 0;
int maxscore = 0;
bool updateScore = false;

TrackSYSCONFIG sysconfig;
TrackDATAFRAME dataframe;
Uint32 DataStartTime = 0;


// Initializes everything and returns true if there were no errors
bool init();
// Sets up OpenGL
void setup_opengl();
// Performs closing operations
void clean_up();
// Draws objects on the screen
void draw_screen();
// Main game loop
void game_update();

//file to load in trial table
int LoadTrFile(char *filename);


int main(int argc, char* args[])
{

	//HIGH_PRIORITY_CLASS
	if (SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS))
		std::cerr << "Promote process priority to Above Normal." << std::endl;
	else
		std::cerr << "Promote process priority failed." << std::endl;

	if (!init())
	{
		// There was an error during initialization
		return 1;
	}

	DataStartTime = SDL_GetTicks();

	while (!quit)
	{
		int inputs_updated = 0;

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

		// Get data from input devices
		if (inputs_updated > 0) // if there is a new frame of data
		{

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


		}
		game_update(); // Run the game loop
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
		return -1;
		//exit(1);
	}
	else
		std::cerr << "Opened TrialFile " << TRIALFILE << std::endl;

	trfile.getline(tmpline,sizeof(tmpline),'\n');

	while(!trfile.eof())
	{
		sscanf(tmpline, "%d %f %f %f %f %f %f %d %d %f %f", &trtbl[ntrials].TrialType, &trtbl[ntrials].startx,&trtbl[ntrials].starty, &trtbl[ntrials].xpos,&trtbl[ntrials].ypos, &trtbl[ntrials].x2pos,&trtbl[ntrials].y2pos, &trtbl[ntrials].holdtime, &trtbl[ntrials].tdeadline, &trtbl[ntrials].velmin, &trtbl[ntrials].velmax);
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



bool init()
{
	// Initialize all experimental parameters

	char tmpstr[80];
	char fname[50] = TRIALFILE;
	int a;

	trackstatus = TrackBird::InitializeBird(&sysconfig);
	if (trackstatus <= 0)
		std::cerr << "Tracker failed to initialize. Mouse Mode.";

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
	//Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096);
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


	// Load files and initialize pointers

	//Image* tgttraces[NTRACES+1];  //is there a limit to the size of this array (stack limit?).  cannot seem to load more than 10 image traces...

	/*
	//load all the trace files
	for (a = 0; a < NTRACES; a++)
	{
		sprintf(tmpstr,"%s/Trace%d.png",TRACEPATH,a);
		tgttraces[a] = Image::LoadFromFile(tmpstr);
		if (tgttraces[a] == NULL)
			std::cerr << "Image Trace" << a << " did not load." << std::endl;
		else
		{
			targettraces[a] = new Object2D(tgttraces[a]);
			targettraces[a]->On();
			std::cerr << "   Trace " << a << " loaded." << std::endl;
		}
	}
	drawstruc.drawtrace = -99;
	*/

	
	//load trial table from file
	NTRIALS = LoadTrFile(fname);
	//std::cerr << "Filename: " << fname << std::endl;
	if(NTRIALS == -1)
	{
		std::cerr << "Trial File did not load." << std::endl;
		return false;
	}
	else
		std::cerr << "Trial File loaded: " << NTRIALS << " trials recorded." << std::endl;
	CurTrial = 0;

	//assign the data-output file name based on the trial-table name 
	std::string savfile;
	savfile.assign(fname);
	savfile.insert(savfile.rfind("."),"_data");
	savfile.erase(savfile.rfind("."),4);

	std::strcpy(fname,savfile.c_str());

	std::cerr << "SavFileName: " << fname << std::endl;

	writer = new DataWriter(&sysconfig,fname);  //set up the data-output file


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

	startCircle = new Circle(curtr.startx, curtr.starty, START_RADIUS*2, startColor);
	startCircle->setBorderWidth(0.001f);
	startCircle->SetBorderColor(blkColor);
	startCircle->On();
	startCircle->BorderOn();

	targCircle1 = new Circle(curtr.xpos, curtr.ypos, TARGET_RADIUS*2, targetColor);
	targCircle1->SetBorderColor(targetColor);
	targCircle1->setBorderWidth(0.002f);
	targCircle1->BorderOn();
	targCircle1->Off();

	targCircle2 = new Circle(curtr.xpos, curtr.ypos, TARGET_RADIUS*2, targetColor);
	targCircle2->SetBorderColor(targetColor);
	targCircle2->setBorderWidth(0.002f);
	targCircle2->BorderOn();
	targCircle2->Off();

	PeakVel = -1;

	// Assign array index 0 of controller and cursor to correspond to mouse control
	//controller[0] = new MouseInput();

	if (trackstatus > 0)
	{
		/* Assign birds to the same indices of controller and cursor that they use
		* for the Flock of Birds
		*/
		for (a = 1; a <= BIRDCOUNT; a++)
		{
			cursors[a] = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*2, cursColor);
			cursors[a]->BorderOff();
			curs[a] = new HandCursor(cursors[a]); 
			curs[a]->SetOrigin(curtr.startx, curtr.starty);
		}

		player = curs[HAND];  //this is the cursor that represents the hand

	}
	else
	{
		// Use mouse control
		cursors[0] = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*2, cursColor);
		curs[0] = new HandCursor(cursors[0]);
		curs[0]->SetOrigin(curtr.startx, curtr.starty);
		player = curs[0];
	}


	hit = new Sound("Resources/coin.wav");
	if (hit == NULL)
		std::cerr << "Sound hit did not load." << std::endl;
	startbeep = new Sound("Resources/startbeep.wav");
	if (startbeep == NULL)
		std::cerr << "Sound startbeep did not load." << std::endl;
	errorbeep = new Sound("Resources/errorbeep1.wav");
	if (errorbeep == NULL)
		std::cerr << "Sound errorbeep did not load." << std::endl;
	errorbeep2 = new Sound("Resources/errorbeep2.wav");
	if (errorbeep2 == NULL)
		std::cerr << "Sound errorbeep2 did not load." << std::endl;

	/* To create text, call a render function from SDL_ttf and use it to create
	* an Image object. See http://www.libsdl.org/projects/SDL_ttf/docs/SDL_ttf.html#SEC42
	* for a list of render functions.
	*/
	font = TTF_OpenFont("Resources/arial.ttf", 28);
	scoretext = new Image(TTF_RenderText_Blended(font, "0 points total", textColor));
	trscoretext = new Image(TTF_RenderText_Blended(font, "1 Point!", textColor));
	latestarttxt = new Image(TTF_RenderText_Blended(font, "Too Late!", textColor));
	velfasttxt = new Image(TTF_RenderText_Blended(font, "Too Fast!", textColor));
	velslowtxt = new Image(TTF_RenderText_Blended(font, "Too Slow!", textColor));
	trialnumfont = TTF_OpenFont("Resources/arial.ttf", 12);
	trialnum = new Image(TTF_RenderText_Blended(trialnumfont, "1", textColor));

	SDL_WM_SetCaption("Go_Before_You_Know", NULL);

	state = Idle; // Set the initial game state

	std::cerr << std::endl << ">>>Initialization complete.<<<" << std::endl << std::endl;

	return true;
}



static void setup_opengl()
{
	glClearColor(1, 1, 1, 0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	//glShadeModel(GL_SMOOTH);   
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

	float size[2], increment;
	glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE,size);
	glGetFloatv(GL_LINE_WIDTH_GRANULARITY, &increment);

	//std::cerr << "Line Width Range: " << size[0] << " - " << size[1] << std::endl;
	//std::cerr << "Line Width Incr: " << increment << std::endl;

	//float lineWidth[2];
	//glGetFloatv(GL_LINE_WIDTH_RANGE, lineWidth);

	//glEnable(GL_TEXTURE_2D);
}

void clean_up()
{

	delete startCircle;
	delete targCircle1;
	delete targCircle2;
	delete hit;
	delete startbeep;
	delete trialnum;
	delete scoretext;
	delete trscoretext;
	delete latestarttxt;
	delete velfasttxt;
	delete velslowtxt;
	delete writer;
	Mix_CloseAudio();
	TTF_CloseFont(font);
	TTF_Quit();
	SDL_Quit();
	if (trackstatus > 0)
		TrackBird::ShutDownBird(&sysconfig);
}

static void draw_screen()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/*
	//draw the trace specified on top of the base trace, centered in the upper portion of the screen
	if (drawstruc.drawtrace >= 0)
	{
		targettraces[drawstruc.drawtrace]->SetPos(curtr.startx,curtr.starty);  //position trace in the upper half of the center screen
		targettraces[drawstruc.drawtrace]->Draw();
		Target.trace = drawstruc.drawtrace;
	}
	else
		Target.trace = -99;
	*/

	//draw the velocity feedback bar
	if (drawstruc.drawvelbar >= 0)
	{
		VelBarFrame->DrawAlign(22*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16,VelBarFrame->GetWidth(),VelBarFrame->GetHeight(),4);
		VelBarWin->DrawAlign(22*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16+(VelBarFrame->GetHeight()/(VELBARMAX-VELBARMIN))*curtr.velmin,VelBarWin->GetWidth(),(VelBarFrame->GetHeight()/(VELBARMAX-VELBARMIN))*(curtr.velmax-curtr.velmin),4);
		VelBar->DrawAlign(22*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16,VelBarFrame->GetWidth()*0.8,VelBarFrame->GetHeight()*drawstruc.drawvelbar,4);	
	}


	//draw the target circle if the drawState is set to 1
	targCircle1->Draw();
	if (targCircle1->drawState())
	/*
	{
		Target.tgtx = targCircle1->GetX();
		Target.tgty = targCircle1->GetY();
	}
	else
	{
		Target.tgtx = -99;
		Target.tgty = -99;
	}
	*/

	//draw the target circle if the drawState is set to 1
	targCircle2->Draw();
	/*
	if (targCircle2->drawState())
	{
		Target.tgtx2 = targCircle2->GetX();
		Target.tgty2 = targCircle2->GetY();
	}
	else
	{
		Target.tgtx2 = -99;
		Target.tgty2 = -99;
	}
	*/

	//draw the start circle if the drawState is set to 1
	startCircle->Draw();
	if (startCircle->drawState())
	{
		Target.startx = startCircle->GetX();
		Target.starty = startCircle->GetY();
	}
	else
	{
		Target.startx = -99;
		Target.starty = -99;
	}


	//draw the cursor
	player->Draw();


	if (drawstruc.drawtext[0] >= 0)
	{
		std::stringstream texts;
		std::stringstream texttrs;
		if (updateScore)
		{
			texts << drawstruc.drawtext[0] << " points!";   // / " << maxscore << " Total Points"; 
			delete scoretext;
			scoretext = new Image(TTF_RenderText_Blended(font, texts.str().c_str(), textColor));

			//texttrs << trscore << " Points!";
			//delete trscoretext;
			//trscoretext = new Image(TTF_RenderText_Blended(font, texttrs.str().c_str(), textColor));

			updateScore = false;

		}

		scoretext->Draw(22*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16-PHYSICAL_HEIGHT*1/32);
	}


	if (drawstruc.drawtext[2] > 0)
	{
		latestarttxt->Draw(PHYSICAL_WIDTH/2,3*PHYSICAL_HEIGHT/4);
	}
	else if (drawstruc.drawtext[1] > 0)
	{
		trscoretext->Draw(PHYSICAL_WIDTH/2,3*PHYSICAL_HEIGHT/4);
	}
	else if (drawstruc.drawtext[3] > 0)
		velfasttxt->Draw(PHYSICAL_WIDTH/2,3*PHYSICAL_HEIGHT/4-1*PHYSICAL_HEIGHT/12);
	else if (drawstruc.drawtext[3] < 0)
		velslowtxt->Draw(PHYSICAL_WIDTH/2,3*PHYSICAL_HEIGHT/4-1*PHYSICAL_HEIGHT/12);


	//write the trial number
	trialnum->Draw(PHYSICAL_WIDTH*23/24, PHYSICAL_HEIGHT*23/24);

	SDL_GL_SwapBuffers();
	glFlush();
}


bool mvonsetgood = false;
bool mvthreshgood = false;
bool hitTarget = false;
bool endTrial = false;
bool setHitTimer = false;
int holdfdbktime = 0;
bool triggerTarget = false;

void game_update()
{
	switch (state)
	{
	case Idle: // wait for player to start trial by entering start circle

		// configure start target
		startCircle->setBorderWidth(0.001f);

		targCircle1->SetBorderColor(targetColor);
		targCircle1->SetColor(whiteColor);
		targCircle2->SetBorderColor(targetColor);
		targCircle2->SetColor(whiteColor);


		Target.TrType = 0;
		Target.velmin = curtr.velmin;
		Target.velmax = curtr.velmax;

		Target.tgtx = -99;
		Target.tgty = -99;
		Target.tgtx2 = -99;
		Target.tgty2 = -99;

		/*
		if (CurTrial == 0)
			drawstruc.drawtrace = -99;
		else
			drawstruc.drawtrace = curtr.trace;
		*/

		drawstruc.drawtext[0] = score;
		drawstruc.drawtext[1] = 0;
		drawstruc.drawtext[2] = 0;
		drawstruc.drawtext[3] = 0;

		Target.score = 0;

		if (player->Distance(startCircle) < START_RADIUS)
		{ 
			hoverTimer = SDL_GetTicks();

			drawstruc.drawtext[1] = 0;

			std::stringstream texttn;
			texttn << CurTrial+1;  //CurTrial starts from 0, so we add 1 for convention.
			delete trialnum;
			trialnum = new Image(TTF_RenderText_Blended(trialnumfont, texttn.str().c_str(), textColor));
			std::cerr << "Trial " << CurTrial+1 << " started at " << SDL_GetTicks() << "." << std::endl;

			state = WaitStart;
		}
		break;

	case WaitStart: // wait for ITI ms before presenting target + GO cue

		//drawstruc.drawtrace = curtr.trace;


			//targCircle1->SetColor(targetColor);
			targCircle1->SetColor(whiteColor);
			targCircle1->SetPos(curtr.xpos+curtr.startx, curtr.ypos+curtr.starty);
			targCircle1->On();
			Target.tgtx = -targCircle1->GetX();
			Target.tgty = -targCircle1->GetY();
			//std::cerr << "Target 1 on." << std::endl;

			if ( (curtr.x2pos > -50) && (curtr.y2pos > -50) )
			{
				//targCircle2->SetColor(targetColor);
				targCircle2->SetColor(whiteColor);
				targCircle2->SetPos(curtr.x2pos+curtr.startx, curtr.y2pos+curtr.starty);
				targCircle2->On();
				Target.tgtx2 = -targCircle2->GetX();
				Target.tgty2 = -targCircle2->GetY();


				//std::cerr << "Target 2 on. " << SDL_GetTicks() << std::endl;

			}


		if (player->Distance(startCircle) > START_RADIUS*1.5)
		{   // go back to Idle if player leaves start circle
			hoverTimer = SDL_GetTicks();
			targCircle1->Off();
			targCircle2->Off();
			Target.tgtx = -99;
			Target.tgty = -99;
			Target.tgtx2 = -99;
			Target.tgty2 = -99;
			state = Idle;
		}
		else if (SDL_GetTicks() - hoverTimer >= curtr.holdtime)
		{

			//if we have hovered long enough, start the trial

			Target.TrType = curtr.TrialType;

			drawstruc.drawvelbar = -1;
			PeakVel = -1;

			mvonsetgood = false;
			mvthreshgood = false;
			hitTarget = false;
			endTrial = false;
			setHitTimer = false;
			triggerTarget = false;

			startbeep->Play();

			gameTimer = SDL_GetTicks();
			trialDone = 0;

			std::cerr << "Going to ShowTgts State." << std::endl;
			state = ShowTgts;
		}
		break;

	case ShowTgts:	// control target displays

		//drawstruc.drawtrace = curtr.trace;

		//update peak velocity variable
		if (player->GetVel() > PeakVel)
			PeakVel = player->GetVel();
		drawstruc.drawvelbar = (PeakVel-VELBARMIN)/(VELBARMAX-VELBARMIN);  //draw the velocity feedback bar; the valid region is the lower half of the bar.
		drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0 : drawstruc.drawvelbar);
		drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1 : drawstruc.drawvelbar);				

		if ((player->GetVel() > VEL_THR) || (player->Distance(startCircle) > 2*TARGET_RADIUS) )
		{

			if (SDL_GetTicks()-gameTimer < curtr.tdeadline)
				mvonsetgood = true;

			hoverTimer = SDL_GetTicks();
		}
		else if (SDL_GetTicks()-gameTimer > curtr.tdeadline)
		{
			//if you don't start the movement on time, cancel the trial! 
			mvonsetgood = false;
			mvthreshgood = false;

			/*

			//we may want to append the trial at the end of the table to repeat it?
			NTRIALS += 1;
			trtbl[NTRIALS].TrialType = curtr.TrialType;
			trtbl[NTRIALS].startx = curtr.startx;
			trtbl[NTRIALS].starty = curtr.starty;
			trtbl[NTRIALS].xpos = curtr.xpos;
			trtbl[NTRIALS].ypos = curtr.ypos;
			trtbl[NTRIALS].x2pos = curtr.x2pos;
			trtbl[NTRIALS].y2pos = curtr.y2pos;
			trtbl[NTRIALS].holdtime = curtr.holdtime;
			trtbl[NTRIALS].tdeadline = curtr.tdeadline;
			trtbl[NTRIALS].velmin = curtr.velmin;
			trtbl[NTRIALS].velmax = curtr.velmax;

			*/

			endTrial = true;
		}

		//we will use a distance threshold to trigger target
		if ( (player->Distance(startCircle) > 0.25f*startCircle->Distance(targCircle1)) ) // || (player->GetVel() > VEL_THR_TRIGGER)    // 5.0f*TARGET_RADIUS // mvonsetgood &&  (1.0f/10.0f)*targCircle1->Distance(startCircle)) )
		{
			if (SDL_GetTicks()-gameTimer < curtr.tdeadline)
				mvthreshgood = true;

			//targCircle2->Off();
			//targCircle2->SetColor(blkColor);
			targCircle1->SetColor(targetColor);
			//targCircle1->On();
			//targCircle2->Off();
			//std::cerr << "Target 2 off. " << SDL_GetTicks() << " " << player->Distance(startCircle) << " " << (1.0f/3.0f)*targCircle1->Distance(startCircle) << " " << targCircle1->GetX() << " " << targCircle1->GetY() << " " << startCircle->GetX() <<" " << startCircle->GetY()  << std::endl;

			Target.tgtx = targCircle1->GetX();
			Target.tgty = targCircle1->GetY();

			triggerTarget = true;

		}

		//if we hit the target, exceed the target circle, or surpass the time-out time, end the trial
		if (player->HitTarget(targCircle1) )
		{

			targCircle1->SetColor(hitColor);
			//targCircle2->Off();
			hitTarget = true;
			hoverTimer = SDL_GetTicks();
			endTrial = true;
		}
		else if ((player->Distance(startCircle) >= targCircle1->Distance(startCircle)) || (SDL_GetTicks()-gameTimer > MOV_TIME) ) //|| (PeakVel > VEL_THR_TRIGGER && player->GetVel() < (PeakVel*1.0f/3.0f) ) )
		{
			endTrial = true;
			if (!setHitTimer)
			{
				hitTimer = SDL_GetTicks();
				setHitTimer = true;
			}
			//targCircle2->Off();
			//targCircle1->Off();
		}
		else if ( (player->Distance(startCircle) >= 3*START_RADIUS && player->GetVel() < (PeakVel*0.6f)) || (triggerTarget && PeakVel < 0.5*curtr.velmin)  )
		{
			//if the velocity is too slow at or after the trigger point, or if the subject slows down and then speeds back up (e.g., test to see which target is right and then go), end the trial
			endTrial = true;
			if (!setHitTimer)
			{
				hitTimer = SDL_GetTicks();
				setHitTimer = true;
			}

		}

		//else if (endTrial && setHitTimer)
		//{
		//	Uint32 timeDiff = SDL_GetTicks() - hoverTimer;
		//	endTrial = true;
		//}


		if (endTrial && ( (setHitTimer && SDL_GetTicks()-hitTimer > 50) || player->HitTarget(targCircle1)) )
		{

			//targCircle1->SetColor(grayColor);
			//targCircle2->SetColor(grayColor);
			//targCircle1->SetBorderColor(grayColor);
			//targCircle2->SetBorderColor(grayColor);


			//if we have started the movement at a good time and have the correct peak velocity, score the trial
			if ((mvthreshgood || mvonsetgood) && (PeakVel > curtr.velmin && PeakVel < curtr.velmax) && hitTarget)
			{
				//drawstruc.drawtext[1] = 1;
				//hit->Play();
				holdfdbktime = 125;
				score += 1;
				Target.score = 1;
				updateScore = true;
			}
			else
			{

				bool playbeep = false;
				holdfdbktime = 75;
				if (!mvonsetgood)
				{
					drawstruc.drawtext[2] = 1;
					errorbeep->Play();
					playbeep = true;
				}
				else
					drawstruc.drawtext[2] = 0;

				if (PeakVel < curtr.velmin)
				{
					drawstruc.drawtext[3] = -1;
					if (!playbeep)
						errorbeep2->Play();
				}
				else if (PeakVel > curtr.velmax)
				{
					errorbeep2->Play();
					if (!playbeep)
						drawstruc.drawtext[3] = 1;
				}
				else
					drawstruc.drawtext[3] = 0;


				drawstruc.drawtext[1] = 0;
			}


			drawstruc.drawtext[0] = score;
			gameTimer = SDL_GetTicks();
			hoverTimer = SDL_GetTicks();

			std::cerr << "Going to Finished State." << std::endl;
			state = Finished;


		}

		break;


	case Finished:

		//targCircle1->Off();
		//targCircle2->Off();


		if (SDL_GetTicks() - gameTimer > holdfdbktime)
		{

			targCircle1->Off();
			targCircle2->Off();

			Target.tgtx = -99;
			Target.tgty = -99;
			Target.tgtx2 = -99;
			Target.tgty2 = -99;

		}


		if (SDL_GetTicks() - gameTimer > HOLD_TIME)
		{
			CurTrial++;

			std::cerr << "Trial " << CurTrial << " completed at " << SDL_GetTicks() << ". " << std::endl;

			//if we are end of the trial table, quit; otherwise start the next trial
			if(CurTrial>= NTRIALS)  
			{   
				hoverTimer = SDL_GetTicks();
				state = Exiting;
				std::cerr << "Going to Exiting State." << std::endl;
			}
			else
			{
				// reset flags

				state = Idle;
				std::cerr << "Going to Idle State." << std::endl;
			}

		}

		break;

	case Exiting:

		player->Null();
		player->On();
		targCircle1->Off();
		targCircle2->Off();
		//drawstruc.drawtrace = -99;
		drawstruc.drawvelbar = -99;
		Target.score = 0;


		if(SDL_GetTicks()-hoverTimer > 5000)
		{
			quit = true;
		}
		break;
	}
}
