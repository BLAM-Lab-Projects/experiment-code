#include <cmath>
#include <sstream>
//#include <iomanip>
#include <iostream>
#include <fstream>
#include <istream>
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

#include <gl/GL.h>
#include <gl/GLU.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "SDL.lib")
#pragma comment(lib, "SDLmain.lib")
#pragma comment(lib, "SDL_mixer.lib")
#pragma comment(lib, "SDL_ttf.lib")
#pragma comment(lib, "SDL_image.lib")
#pragma comment(lib, "Bird.lib")
#pragma push(1)

/* 
	Sequence learning paradigm, with prediction allowed
	   -Modified from KineReachDemo code (Promit and Aneesh), 9/10/2012
	   -Originally written to perform the sequence learning task where the initial position is prompted, 
		   but the remaining targets are not.
	   -General functionality includes loading a paradigm from a trial table, reading in trace files, and executing the experiment
	   -Data-saving functionality expanded to include saving target position information

*/

enum GameState
{
	Idle = 0x01,     //00001
	Starting = 0x03, //00011
	Active = 0x04,   //00100
	Ending = 0x0C,   //01100
	Finished = 0x10  //10000
};




SDL_Event event;
SDL_Surface* screen = NULL;
/* COM ports where the Flock of Birds are connected. The first element is always
 * 0, and subsequent elements are the port numbers in order.
 */
//WORD COM_port[5] = {0, 7, 6, 5, 8};
//WORD COM_port[5] = {0, 6,8,7,5};
WORD COM_port[5] = {0, 5,6,7,8};
InputDevice* controller[BIRDCOUNT + 1];
Object2D* cursor[BIRDCOUNT + 1];
Object2D* starttgt = NULL;
Object2D* target = NULL;
Object2D* player = NULL;
Object2D* targettraces[NTRACES];
Image* text = NULL;
Sound* scorebeep = NULL;
Sound* startbeep = NULL;
TTF_Font* font = NULL;
SDL_Color textColor = {0, 0, 0};
DataWriter* writer = NULL;
GameState state;
Uint32 gameTimer;
Uint32 hoverTimer;
bool birds_connected;

BIRDSYSTEMCONFIG sysconfig;

//bool updatedisplay = true;

//variables to compute the earned score
int score = 0;
int seqscore = 0;
int seqcompleted = 0;


// Trial table.
typedef struct {
	int NewTrial;			// Flag 1-for new trial.
	float startx,starty;		// x/y pos of start target; also, trace will be centered here!
	float xpos,ypos;		// x/y pos of target.
	int duration;			// duration of target.
	int trace;
} TRTBL;

#define TRTBL_SIZE 1000
TRTBL trtbl [TRTBL_SIZE];


int NTRIALS = 0;
int CurTrial = 0;

#define curtr trtbl[CurTrial]

//char filename[30] = TRIALFILE;

//target structure; keep track of where the current target is now (only 1!)
TargetFrame Target;


typedef struct {
	int drawbird[BIRDCOUNT + 1];
	int drawtrace;
	bool drawstart;
	bool drawtgt;
	bool drawtext;
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

// Main game loop
void game_update();


int main(int argc, char* args[])
{
	std::cerr << "Start main." << std::endl;
	if (!init())
	{
		// There was an error during initialization
		std::cerr << "Initialization error." << std::endl;
		return 1;
	}
	
	bool quit = false;
	while (!quit)
	{
		game_update(); // Run the game loop

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

		//if ((CurTrial >= NTRIALS) && ( (state == Idle) || (state == Finished)) )
		//	quit = true;

		// Get data from input devices
		if (inputs_updated) // if there is a new frame of data
		{
			//updatedisplay = true;
			for (int a = (birds_connected ? 1 : 0); a <= (birds_connected ? BIRDCOUNT : 0); a++)
			{
				InputFrame i = controller[a]->GetFrame();
				writer->Record(a, i, Target);
				cursor[a]->SetPos(i.x, i.y);
				cursor[a]->SetAngle(i.theta);
			}
		}

		//if (updatedisplay)  //reduce number of calls to draw_screen -- does this speed up display/update?
		draw_screen();

	}

	clean_up();
	//return 0;
}





int LoadTrFile(char *fname)
{

	//std::cerr << "LoadTrFile begin." << std::endl;

	char tmpline[50] = ""; 
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
	trfile.close();
	
	std::strcpy(fname,tmpline);

	//std::cerr << "Read in: " << fname << std::endl;


	//read in the trial table
	std::ifstream tablefile(tmpline);
	//std::ifstream tablefile(fname);

	if (!tablefile)
	{
		std::cerr << "Cannot open input file." << std::endl;
		return -1;
		//exit(1);
	}
	else
		std::cerr << "Opened TrialFile " << tmpline << std::endl;

	tablefile.getline(tmpline,sizeof(tmpline),'\n');

	while(!tablefile.eof())
	{
			sscanf(tmpline, "%d %f %f %f %f %d %d",&trtbl[ntrials].NewTrial,&trtbl[ntrials].startx, &trtbl[ntrials].starty,&trtbl[ntrials].xpos, &trtbl[ntrials].ypos, &trtbl[ntrials].duration, &trtbl[ntrials].trace);
				 //&trtbl[ntrials].NewTrial, &trtbl[ntrials].xpos, &trtbl[ntrials].ypos, &trtbl[ntrials].type, &trtbl[ntrials].duration, &trtbl[ntrials].tgton, &trtbl[ntrials].toneon);
			ntrials++;
			tablefile.getline(tmpline,sizeof(tmpline),'\n');
	}

	tablefile.close();
	if(ntrials == 0)
	{
		std::cerr << "Empty input file." << std::endl;
		//exit(1);
		return -1;
	}
	return ntrials;
}




bool init()
{
	// Initialize Flock of Birds
	/* The program will run differently if the birds fail to initialize, so we
	 * store it in a bool.
	 */

	int a;
	char tmpstr[30];
	//char teststr[30];
	char fname[50] = TRIALFILE;
	int b;

	//std::cerr << "Start init." << std::endl;

	birds_connected = init_fob();
	//birds_connected = false; //override this for now, for testing purposes.
	if (!birds_connected)
		std::cerr << "No birds initilalized. Mouse mode." << std::endl;

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

	setup_opengl();
	Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096);
	if (TTF_Init() == -1)
	{
		std::cerr << "Audio failed to initialize." << std::endl;
		return false;
	}
	else
		std::cerr << "Audio initialized." << std::endl;

	//turn off the cursor
	SDL_ShowCursor(0);

	// Load files and initialize pointers
	font = TTF_OpenFont("Resources/arial.ttf", 28);
	Image* crosshair = Image::LoadFromFile("Resources/crosshair.png");
	if (crosshair == NULL)
		std::cerr << "Image crosshair did not load." << std::endl;

	Image* tgttraces[NTRACES+1];  //is there a limit to the size of this array (stack limit?).  cannot seem to load more than 10 image traces...

	//load all the trace files
	for (a = 1; a <= NTRACES; a++)
	{
		sprintf(tmpstr,"Resources/Trace%d.png",a);
		tgttraces[a-1] = Image::LoadFromFile(tmpstr);
		if (tgttraces[a-1] == NULL)
			std::cerr << "Image Trace" << a << " did not load." << std::endl;
		else
		{
			targettraces[a-1] = new Object2D(tgttraces[a-1]);
			std::cerr << "   Trace " << a << " loaded." << std::endl;
			//targettraces[a]->SetPos(PHYSICAL_WIDTH / 2, PHYSICAL_HEIGHT / 2);
		}
	}

	Image* ctgt = Image::LoadFromFile("Resources/ctgt.png");
	if (ctgt == NULL)
			std::cerr << "Image ctgt did not load." << std::endl;

	Image* tgt = Image::LoadFromFile("Resources/tgt.png");
	if (tgt == NULL)
			std::cerr << "Image tgt did not load." << std::endl;

	std::cerr << "Images loaded: " << a-1 << "." << std::endl;

	//load table from file
	NTRIALS = LoadTrFile(fname);
	//char trialfile[30];	
	//NTRIALS = LoadTrFile(trialfile);

	//std::cerr << "Filename: " << fname << std::endl;
	
	if(NTRIALS == -1)
	{
		std::cerr << "Trial File did not load." << std::endl;
		return false;
	}
	else
		std::cerr << "Trial File loaded: " << NTRIALS << " trials found." << std::endl;

	std::string savfile;
	savfile.assign(fname);
	savfile.insert(savfile.rfind("."),"_data");

	std::strcpy(fname,savfile.c_str());

	std::cerr << "SavFileName: " << fname << std::endl;



	// Assign array index 0 of controller and cursor to correspond to mouse control
	controller[0] = new MouseInput();
	cursor[0] = new Object2D(crosshair);
	if (birds_connected)
	{
		/* Assign birds to the same indices of controller and cursor that they use
		 * for the Flock of Birds
		 */

		//drawstruc.drawbird[0] = 0;

		for (a = 1; a <= BIRDCOUNT; a++)
		{
			controller[a] = new BirdInput(a);
			cursor[a] = new Object2D(crosshair);
			//drawstruc.drawbird[a] = 0;
		}
		// Use bird 1
		//cursor[1] = new Object2D(crosshair);
		player = cursor[3];  //right hand is bird #3
		drawstruc.drawbird[3] = 1;

		//if the birds are active, shut off the cursor
		//SDL_ShowCursor(0);
	}
	else
	{
		// Use mouse control
		player = cursor[0];
		drawstruc.drawbird[0] = 1;
		sysconfig.dMeasurementRate = 0;
	}	
	starttgt = new Object2D(ctgt);
	target = new Object2D(tgt);


	//phaser = new Sound("Resources/phaser.wav");
	scorebeep = new Sound("Resources/correctbeep.wav");
	startbeep = new Sound("Resources/startbeep.wav");

	/* To create text, call a render function from SDL_ttf and use it to create
	 * an Image object. See http://www.libsdl.org/projects/SDL_ttf/docs/SDL_ttf.html#SEC42
	 * for a list of render functions.
	 */
	text = new Image(TTF_RenderText_Blended(font, " ", textColor));

	writer = new DataWriter(&sysconfig,fname);

	SDL_WM_SetCaption("SequenceLearningTask2", NULL);

	//starttgt->SetPos(PHYSICAL_WIDTH / 2, PHYSICAL_HEIGHT / 2); // Set the start marker in the center
	state = Idle; // Set the initial game state
	std::cerr << "initialization complete." << std::endl;
	return true;
}

bool init_fob()
{
	if (birdRS232WakeUp(GROUP_ID, FALSE, BIRDCOUNT, COM_port, BAUD_RATE,
		READ_TIMEOUT, WRITE_TIMEOUT, GMS_GROUP_MODE_NEVER))
	{
		if (birdStartFrameStream(GROUP_ID))
		{
			
			//set up system configuration
			birdGetSystemConfig(GROUP_ID,&sysconfig); //get defaults
			sysconfig.dMeasurementRate = SAMPRATE;  //edit sampling rate
			birdSetSystemConfig(GROUP_ID,&sysconfig); //set sampling rate
			birdGetSystemConfig(GROUP_ID,&sysconfig); //re-update saved values

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
}

void clean_up()
{
	for (int a = 0; a <= (birds_connected ? BIRDCOUNT : 0); a++)
	{
		delete cursor[a];
		delete controller[a];
	}
	delete starttgt;
	delete target;
	delete scorebeep;
	delete startbeep;
	
	for (int a = 0; a < NTRACES; a++)
		delete targettraces[a];
	
	delete text;
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

static void draw_screen()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	/*
	// Draw the mouse cursor, as well as all the birds if available
	for (int a = 0; a <= (birds_connected ? BIRDCOUNT : 0); a++)
	{
		cursor[a]->Draw();
	}
	*/

	//draw the trace specified
	if (drawstruc.drawtrace >= 0)
	{
		//targettraces[drawstruc.drawtrace]->Draw(PHYSICAL_WIDTH, PHYSICAL_HEIGHT);
		targettraces[drawstruc.drawtrace]->Draw();
		Target.trace = drawstruc.drawtrace+1;
	}
	else
		Target.trace = -1;


	// Draw the start marker, if true
	if (drawstruc.drawstart)
	{
		starttgt->Draw(START_RADIUS * 2, START_RADIUS * 2);
		Target.startx = starttgt->GetX();
		Target.starty = starttgt->GetY();
		//Target.rWin = START_RWIN;

		//drawstruc.drawstart = false;
	}
	else
	{
		Target.startx = -100;
		Target.starty = -100;
	}



	// Draw the target marker for the current trial, if true
	if (drawstruc.drawtgt)
	{
		// Marker is stretched to the activation radius
		//target->SetPos(curtr.xpos, curtr.ypos);
		target->Draw(TARGET_RADIUS * 2, TARGET_RADIUS * 2);
		Target.tgtx = target->GetX();
		Target.tgty = target->GetY();
		//Target.rWin = TARGET_RWIN;

		//drawstruc.drawtgt = false;
	}
	else
	{
		Target.tgtx = -100;
		Target.tgty = -100;
	}


	//draw only the mouse/birds requested, as initialized in init()
	for(int a = 0; a < (birds_connected ? BIRDCOUNT : 1); a++)
	{
		if (drawstruc.drawbird[a] == 1)  //only draw if the bird drawflag is set
		{
			cursor[a]->Draw(CURSOR_RADIUS * 2, CURSOR_RADIUS * 2);
		}
	}


	// Draw text
	if (drawstruc.drawtext)
	{
		std::stringstream scorestring;
			scorestring << "You earned " 
				        << score 
						<< " points.";
			text = new Image(TTF_RenderText_Blended(font, scorestring.str().c_str(), textColor));
			text->Draw(0.6f, 0.6f);
			scorestring.str("");
			scorestring << "+ "
						<< seqcompleted
						<< " sequences x 5 = "
						<< seqcompleted*5
						<< " bonus points!";
			text = new Image(TTF_RenderText_Blended(font, scorestring.str().c_str(), textColor));
			text->Draw(0.6f, 0.57f);
			scorestring.str("");
			scorestring << "Total score: "
					    << score+seqcompleted*5
						<< " points!";
			text = new Image(TTF_RenderText_Blended(font, scorestring.str().c_str(), textColor));
			text->Draw(0.6f, 0.5f);
	}

	SDL_GL_SwapBuffers();
	glFlush();

	//updatedisplay = false;
}


Uint32 TstartTrial = 0;
bool reachedtgtflag = false;
bool starttgtflag = false;

void game_update()
{


	switch (state)
	{
		case Idle:
			/* If player starts hovering over start marker, set state to Starting
			 * and store the time -- this is for trial #1 only!
			 */
			drawstruc.drawtrace = -1; //for normal operations, show no trace
			//drawstruc.drawtrace = 0;  //for calibration purposes, display trace0 always
			starttgt->SetPos(curtr.startx, curtr.starty);
			drawstruc.drawstart = true;
			drawstruc.drawtgt = false;
			drawstruc.drawtext = false;

			if( (Object2D::Distance(player, starttgt) <= START_RADIUS) && (CurTrial < NTRIALS) )
			{
				targettraces[curtr.trace-1]->SetPos(curtr.startx, curtr.starty);
				//drawstruc.drawtrace = 0;
				drawstruc.drawtrace = curtr.trace-1;
				hoverTimer = SDL_GetTicks();
				std::cerr << "Leaving IDLE state." << std::endl;
				state = Starting;
				starttgtflag = false;
				//updatedisplay = true;
			}
			break;
		case Starting:
			/* If player stops hovering over start marker, set state to Idle and
			 * store the time  -- this is for new trials only!
			 */
			targettraces[curtr.trace-1]->SetPos(curtr.startx, curtr.starty);
			drawstruc.drawtrace = curtr.trace-1;
			starttgt->SetPos(curtr.startx, curtr.starty);
			drawstruc.drawstart = true;
			drawstruc.drawtgt = false;

			if (Object2D::Distance(player, starttgt) > START_RADIUS)
			{
				//updatedisplay = true;
				starttgtflag = false;
				state = Idle;
			}
			// If player hovers long enough, set state to Active
			else if (SDL_GetTicks() - hoverTimer >= WAITTIME)
			{
				startbeep->Play();
				gameTimer = SDL_GetTicks();
				TstartTrial = SDL_GetTicks();
				target->SetPos(curtr.xpos, curtr.ypos);
				drawstruc.drawtgt = true;
				reachedtgtflag = false;
				starttgtflag = true;
				seqscore = 0;
				std::cerr << "Leaving STARTING state." << std::endl;
				state = Active;
				//updatedisplay = true;
			}
			break;
		case Active:
			
			//targettraces[curtr.trace]->SetPos(curtr.startx, curtr.starty);
			drawstruc.drawtrace = curtr.trace-1;
			drawstruc.drawstart = true;

			if ( (Object2D::Distance(player, starttgt) <= TARGET_RADIUS) )
				starttgtflag = true;
			


			if ((SDL_GetTicks() - TstartTrial) > curtr.duration)
			{
				//updatedisplay = true;
				CurTrial++;
				//std::cerr << "Trial " << CurTrial << " completed." << std::endl;
				std::cerr << "Trial " << CurTrial << " ended at " << SDL_GetTicks() 
					<< ". Elapsed time, " << (SDL_GetTicks() - TstartTrial) << std::endl;
				
				//check for score bonus
				if (seqscore == 5)
					seqcompleted++;

				//if we have reached the end of the trial table, quit
				if (CurTrial >= NTRIALS)
				{
					std::cerr << "Leaving ACTIVE state to FINISHED state." << std::endl;
					state = Finished;
				}
				else if (curtr.NewTrial == 1)
				{
					std::cerr << "Leaving ACTIVE state to STARTING state." << std::endl;
					hoverTimer = SDL_GetTicks();
					state = Starting;
				}
				else
				{
					//Continue with the next trial
					TstartTrial = SDL_GetTicks();
					targettraces[curtr.trace-1]->SetPos(curtr.startx, curtr.starty);
					drawstruc.drawtrace = curtr.trace-1;
					starttgt->SetPos(curtr.startx, curtr.starty);
					target->SetPos(curtr.xpos, curtr.ypos);
					drawstruc.drawtgt = true;
					reachedtgtflag = false;
					//starttgtflag = false;
					//startbeep->Play();
					//state = Active;
				}
			}
			
			
			//if the target is intersected, and you previously were in the start target, play success beep
			if ( (Object2D::Distance(player, target) <= TARGET_RADIUS) && (reachedtgtflag == false) && starttgtflag )
			{
				hoverTimer = SDL_GetTicks();  //time the target is achieved
				scorebeep->Play();
				score++;
				drawstruc.drawtgt = false;  //if achieve the target, turn it off to encourage return to start
				starttgtflag = false;
				reachedtgtflag = true;
				seqscore++;
				//updatedisplay = true;
				std::cerr << "Target Intersected." << std::endl;
			}
			

			/*
			//if target is intersected and then the cursor returns to the start position before end of the trial, play success beep
			if ( (Object2D::Distance(player, target) <= TARGET_RADIUS) && (reachedtgtflag == false) )
			{
				hoverTimer = SDL_GetTicks();  //time the target is achieved
				reachedtgtflag = true;
				std::cerr << "Target Intersected." << std::endl;
			}
			if ( (Object2D::Distance(player, starttgt) <= START_RADIUS) && (reachedtgtflag == true) )
			{
				drawstruc.drawtgt = false;  //if achieve the target, turn it off to encourage return to start
				scorebeep->Play();
			}
			*/


			break;
		
		/*case Ending:
			// If player hovers long enough, set state to Finished
			else if (SDL_GetTicks() - hoverTimer >= 200)
			{
				//phaser->Play();
				scorebeep->Play();
				std::stringstream speed;
				speed << SDL_GetTicks() - gameTimer << "ms";
				text = new Image(TTF_RenderText_Blended(font, speed.str().c_str(), textColor));
				gameTimer = SDL_GetTicks();
				state = Finished;
			}
			break;
			*/
		case Finished:
			// Trial table ended, wait for program to quit
			/*
			if (SDL_GetTicks() - gameTimer >= 5000)
			{
				std::cerr << "Leaving FINISHED state." << std::endl;
				state = Idle;
			}
			*/
			drawstruc.drawtgt = false;
			drawstruc.drawstart = false;
			drawstruc.drawtrace = -1;
			
			drawstruc.drawtext = true;

			break;
			


	}
}

