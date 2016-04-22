/* Control software for kinereach
See readme.txt for further details

Authors:
	Adrian Haith
	Promit Roy
	Aneesh Roy
	Aaron Wong

	SMARTS2 software package written by Aaron Wong, last modified 8/6/2014
	        This code was based in large part on the KinereachDemo code version written by Adrian Haith.
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
#include "MouseInput.h"
#include "BirdInput.h"
#include "Object2D.h"
#include "Sound.h"
#include "Circle.h"
#include "HandCursor.h"
#include "TargetFrame.h"
#include "Path2D.h"
#include "JoystickInput.h"

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
	Reach = 0x04,   //00100
	Return = 0x05,
	WaitStartReturn = 0x0C,   //01100
	Finished = 0x10,  //10000
	WaitGo = 0x07,
	Exiting = 0x08,
};

SDL_Event event;
SDL_Surface* screen = NULL;
/* COM ports where the Flock of Birds are connected. The first element is always
 * 0, and subsequent elements are the port numbers in order.
 */
bool quit = false;
WORD COM_port[5] = {0, 5, 6, 7, 8};
InputDevice* controller[BIRDCOUNT + 1];
Circle* cursors[BIRDCOUNT + 1];
HandCursor* curs[BIRDCOUNT + 1];
HandCursor* player = NULL;
Object2D* targettraces[NTRACES+1];
Path2D landoltc[NPATHS+1];
GLfloat failColor[3] = {1.0f, 0.0f, 0.0f};
Image* trialnum = NULL;
Image* scoretext = NULL;
Image* scorebtext = NULL;
bool updateScore = false;
Sound* hit = NULL;
Sound* beep = NULL;
Sound* neutralfdbk = NULL;
TTF_Font* font = NULL;
TTF_Font* trialnumfont = NULL;
SDL_Color textColor = {255, 255, 255};
DataWriter* writer = NULL;
GameState state;
Uint32 gameTimer;
Uint32 hoverTimer;
Uint32 ITI_Timer;
Uint32 movTimer;
Uint32 textTimer;
bool diddiscrim = false;
bool birds_connected;
SDL_Joystick *Joystick = NULL;
bool joystickconnected;
int jsbutton;
Uint32 jsbtnTimer;
bool subjresp = false;

// Trial table structure, to keep track of all the parameters for each trial of the experiment
typedef struct {
	int TrialType;			// Flag for type of trial: 1 = Extent task, 2 = point-to-point task
	float startx,starty;	// x/y pos of start target; also, trace will be centered here!  
	float xpos,ypos;		// x/y pos of the target.
	int rotation;			// target rotation, in degrees
	int trace;				//trace (>= 0) for cuing how to draw the path; if value is <0 no trace will be displayed.  Traces should be numbered sequentially starting from 0.
	int iti;				//inter-trial interval
	int path;				//path number to show at the specified extent
	float pathext;			//extent that must be surpassed to show the path
	int fdbkflag;			//flag to shut off feedback
} TRTBL;

#define TRTBL_SIZE 1000
TRTBL trtbl [TRTBL_SIZE];

int NTRIALS = 0;
int CurTrial = 0;

#define curtr trtbl[CurTrial]


//structure to keep track of what to draw in the draw_screen() function
typedef struct {
	int drawtrace;				  //trace number to draw (for intro screen)
	int drawpath;				  //path number to draw (for discrimination purposes), centered on the cursor
	int drawtext[5];              //write feedback text, depending on what flags are set
	float drawvelbar;             //velocity feedback-bar parameter
	bool drawRing;				  //draw the ring to help guide the subject back to the start target
} DRAWSTRUC;

DRAWSTRUC drawstruc;

float PeakVel;  //constant to keep track of the peak velocity in the trial

//std::ofstream tFileCopy;
//bool recordingStarted;
bool findStart;
bool targetHit;
bool reachEnd;

Circle* targCircle;
Circle* targRotCircle;
Circle* cursCircle;
Circle* handCircle;
Circle* cursKRCircle;
Circle* handKRCircle;
Circle* startCircle;
float RotMat[4];
Path2D returnRing;

//tracking variables for providing KR
float HandPosFdbk[50][2];
float CursPosFdbk[50][2];
int didhandfdbk = 0; //index into HandPosFdbk array - number of captured samples
bool recordhand; //flag to indiate when to record the hand -- set in state machine to only capture outward phase of reach


float targetColor[3] = {0.0f, 1.0f, 0.0f};
float hitColor[3] = {1.f, 1.f, .0f};
float cursColor[3] = {.0f, .0f, 1.0f};
float startColor[3] = {.7f,.7f,.7f};
float blkColor[3] = {0.0f, 0.0f, 0.0f};
float whiteColor[3] = {1.0f, 1.0f, 1.0f};
float handColor[3] = {.0f, 1.0f, 0.5f};

int trialDone = 0;

//target structure; keep track of where the current target is now (only 1!)
TargetFrame Target;

//velocity-bar images
Image* VelBarFrame;
Image* VelBarWin;
Image* VelBar;

//variables to compute the earned score
int score = 0;
bool gotscore = false;
bool gotvel = false;
int respscore = 0;

BIRDSYSTEMCONFIG sysconfig;
//std:: wstringstream path;


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
// Main game loop
void game_update();

//update feedback cursor position
void cursor_feedback(Circle *c,float cHist[50][2]);

/*
// Checks whether a given directory exists
bool dirExists(const std::string& dirName_in);
bool fExists(const std::string& dirName_in);
*/

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
	
	while (!quit)
	{
		bool inputs_updated = false;

		if (joystickconnected && ((SDL_GetTicks()-jsbtnTimer) > 200))
		{
			//jsbutton = -1;
			Target.jsbutton = -1;
		}

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
			else if (joystickconnected && event.type == SDL_JOYBUTTONDOWN)
			{
				jsbutton = event.jbutton.button;
				Target.jsbutton = jsbutton;
				subjresp = true;
				jsbtnTimer = SDL_GetTicks();
				std::cerr << "JS Button Pressed: " << jsbutton << std::endl;
			}
			else if (event.type == SDL_QUIT)
			{
				quit = true;
			}
		}

		// Get data from input devices
		if (inputs_updated) // if there is a new frame of data
		{

			for (int a = (birds_connected ? 1 : 0); a <= (birds_connected ? BIRDCOUNT : 0); a++)
			{
				InputFrame i = controller[a]->GetFrame();

				/*
				if (a == (birds_connected ? HAND : 0))
				{
					player->UpdatePos(i.x, i.y);
					i.vel = player->GetVel();
				}
				*/

				curs[a]->UpdatePos(i.x,i.y);
				i.vel = curs[a]->GetVel();

				i.rotx = curs[a]->GetX();
				i.roty = curs[a]->GetY();

				//if (a == HAND)
				//{
				//	handCircle->SetPos(curs[a]->GetTrueX(),curs[a]->GetTrueY());
				//}

				writer->Record(a, i, Target);
			}

			handCircle->SetPos(player->GetTrueX(),player->GetTrueY());

			if (recordhand && didhandfdbk<50 && (fabs(player->Distance(startCircle))> 0.7*fabs(startCircle->Distance(targCircle))) && (fabs(player->Distance(startCircle)) <= fabs(startCircle->Distance(targCircle))) )  //fabs(player->Distance(startCircle) - startCircle->Distance(targCircle)) <= TARGET_RADIUS*2.5 )
				{
					//record hand positions as the hand moves outward, crossing the threshold
					CursPosFdbk[didhandfdbk][0] = player->GetX();
					CursPosFdbk[didhandfdbk][1] = player->GetY();

					HandPosFdbk[didhandfdbk][0] = handCircle->GetX();
					HandPosFdbk[didhandfdbk][1] = handCircle->GetY();

					didhandfdbk++;
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

	trfile.getline(tmpline,sizeof(tmpline),'\n');  //get the first line of the file, which is the name of the trial-table file
	trfile.close();
	
	std::strcpy(fname,tmpline);

	//std::cerr << "Read in: " << fname << std::endl;


	//read in the real trial table
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
		sscanf(tmpline, "%d %f %f %f %f %d %d %d %d %f %d", &trtbl[ntrials].TrialType, &trtbl[ntrials].startx,&trtbl[ntrials].starty, &trtbl[ntrials].xpos,&trtbl[ntrials].ypos, &trtbl[ntrials].rotation, &trtbl[ntrials].trace, &trtbl[ntrials].iti, &trtbl[ntrials].path,&trtbl[ntrials].pathext,&trtbl[ntrials].fdbkflag); //&trtbl[ntrials].ctime
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
	std::cerr << ntrials << " trials found. " << std::endl;
	return ntrials;
}


void cursor_feedback(Circle *c,float cHist[50][2])
{

	//float sumx = 0;
	//float sumy = 0;
	//float sumxy = 0;
	//float sumx2 = 0;
	//float sumy2 = 0;
	float x1int,x2int,y1int,y2int;
	double t1,t2;

	//draw the cursor at the feedback position

	if (didhandfdbk > 1 && didhandfdbk <= 50)  //if recorded a valid series of observations
	{
		/*
		//if the slope is near infinity, the calculation often returns the wrong answer. so we won't use this method for now.

		//calculate the equation of the regression line approximating the hand path
		for (int a = 0; a < didhandfdbk; a++)
		{
			sumx = sumx + cHist[a][0];
			sumy = sumy + cHist[a][1];
			sumxy = sumxy + cHist[a][0]*cHist[a][1];
			sumx2 = sumx2 + powf(cHist[a][0],2.0f);
			sumy2 = sumy2 + powf(cHist[a][1],2.0f);
			//std::cerr << "HPF" << a << ": " << HandPosFdbk[a][0] << " " << HandPosFdbk[a][1] << std::endl;
		}

		float m = ((didhandfdbk)*sumxy - sumx*sumy)/((didhandfdbk)*sumx2 - powf(sumx,2.0f));
		float c = (sumx2*sumy - sumx*sumxy)/((didhandfdbk)*sumx2 - powf(sumx,2.0f));

		//now we want the intersection of that line and the circle of the target radius. we are guaranteed by the situation to have 2 intersection points.
		//The equation of the circle is (x-p)^2 + (y-q)^2 = r^2.

		float p = startCircle->GetX();
		float q = startCircle->GetY();
		float r2 = powf(startCircle->Distance(targCircle),2.0f);
		float A = (m*m+1.0f);
		float B = 2*(m*c-m*q-p);
		float C = (q*q-r2+p*p-2.0f*c*q+c*c);

		x1int = (-B+sqrt(B*B-4*A*C))/(2.0f*A);
		x2int = (-B-sqrt(B*B-4*A*C))/(2.0f*A);
		y1int = m*x1int+c;
		y2int = m*x2int+c;

		//std::cerr << "Fdbk: " << sumx << " " << sumy << " " << sumx2 << " " << sumy2 << " " << sumxy << " " << sumy << std::endl;
		//std::cerr << "   " << m << " " << c << " " << A << " " << B << " " << C << std::endl;
		*/

		//parameterized version:  this version is stable to vertical targets, but the radius is off (and varies with the speed of the movement?!)

		float p = startCircle->GetX();
		float q = startCircle->GetY();
		float r2 = powf(startCircle->Distance(targCircle),2.0f);  //-TARGET_RADIUS/2.0f
		double A = powf(cHist[didhandfdbk-1][0]-cHist[didhandfdbk-2][0],2.0f) + powf(cHist[didhandfdbk-1][1]-cHist[didhandfdbk-2][1],2.0f);
		double B = 2*(cHist[didhandfdbk-2][0]-p)*(cHist[didhandfdbk-1][0]-cHist[didhandfdbk-2][0]) + 2*(cHist[didhandfdbk-2][1]-q)*(cHist[didhandfdbk-1][1]-cHist[didhandfdbk-2][1]);
		double C = powf((cHist[didhandfdbk-2][0]-p),2.0f) + powf((cHist[didhandfdbk-2][1]-q),2.0f) - r2;

		t1 = (-B+sqrt(B*B-4*A*C))/(2.0f*A);
		t2 = (-B-sqrt(B*B-4*A*C))/(2.0f*A);
		
		x1int = cHist[didhandfdbk-2][0]+t1*(cHist[didhandfdbk-1][0]-cHist[didhandfdbk-2][0]);
		y1int = cHist[didhandfdbk-2][1]+t1*(cHist[didhandfdbk-1][1]-cHist[didhandfdbk-2][1]);

		x2int = cHist[didhandfdbk-2][0]+t2*(cHist[didhandfdbk-1][0]-cHist[didhandfdbk-2][0]);		
		y2int = cHist[didhandfdbk-2][1]+t2*(cHist[didhandfdbk-1][1]-cHist[didhandfdbk-2][1]);

	}
	else  //not enough valid observations; don't show the target anywhere meaningful.
	{
		t1 = 89.0f;
		t2 = -90.0f;

		x1int = 90.0f;
		y1int = 90.0f;

		x2int = -90.0f;
		y2int = -90.0f;
	}

	//std::cerr << "   " << t1 << " " << x1int << " " << y1int << " " << t2 << " " << x2int << " " << y2int << std::endl;
	//std::cerr << "   " << didhandfdbk << std::endl;

	//we take the x,y value that is closest to the last recorded hand position target
	if ((powf(cHist[didhandfdbk-1][0]-x1int,2.0f)+powf(cHist[didhandfdbk-1][1]-y1int,2.0f)) <= (powf(cHist[didhandfdbk-1][0]-x2int,2.0f)+powf(cHist[didhandfdbk-1][1]-y2int,2.0f)))
	//if (fabs(t1)<fabs(t2))
	{		
		c->SetPos(x1int,y1int);
	}
	else
	{
		c->SetPos(x2int,y2int);
	}


}


bool init()
{
	// Initialize Flock of Birds
	/* The program will run differently if the birds fail to initialize, so we
	 * store it in a bool.
	 */

	char tmpstr[80];
	char fname[50] = TRIALFILE;
	int a;

	birds_connected = init_fob();
	if (!birds_connected)
		std::cerr << "No birds initialized. Mouse mode." << std::endl;

	if (SDL_Init(SDL_INIT_JOYSTICK ) < 0)
    {
        //fprintf(stderr, "Couldn't initialize Joystick: %s\nMouse mode.", SDL_GetError());
		std::cerr << "Couldn't initialize Joystick: " << SDL_GetError() << std::endl;
		std::cerr << "Entering mouse mode." << std::endl;
		Joystick = NULL;
        joystickconnected = false;
		jsbutton = -1;
    }
	else
	{
		std::cerr << SDL_NumJoysticks() << " joysticks were found." << std::endl;

		//by default we will use the first joystick found, which is index 0.
		SDL_JoystickEventState(SDL_ENABLE);
		Joystick = SDL_JoystickOpen(0);
		if (Joystick == NULL)
			std::cerr << "Joystick could not be opened." << std::endl;
		else

		std::cerr << "Joystick has " << SDL_JoystickNumAxes(Joystick) << " axes." << std::endl;
		joystickconnected = true;
		jsbutton = -1;
	}

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

	//Image* bird = Image::LoadFromFile("Resources/bird.png");
	//if (bird == NULL)
	//	std::cerr << "Image bird did not load." << std::endl;

	//Image* smallCirc = Image::LoadFromFile("Resources/cursor.png");
	//if (smallCirc == NULL)
	//	std::cerr << "Image smallCirc did not load." << std::endl;
	//Image* startCirc = Image::LoadFromFile("Resources/circle2.png");

	Image* tgttraces[NTRACES+1];  //is there a limit to the size of this array (stack limit?).  cannot seem to load more than 10 image traces...

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
			//targettraces[a]->SetPos(PHYSICAL_WIDTH / 2, PHYSICAL_HEIGHT / 2);
		}
	}
	drawstruc.drawtrace = -99;


	//load all the path files
	for (a = 0; a < NPATHS; a++)
	{
		sprintf(tmpstr,"%s/Path%d.txt",PATHPATH,a);
		landoltc[a] = Path2D::LoadPathFromFile(tmpstr);
		if (CURSDISCRIM)
			landoltc[a].SetPathColor(whiteColor);
		if (landoltc[a].GetPathNVerts() < 0)
			std::cerr << "   Path " << a << " did not load." << std::endl;
		else
			std::cerr << "   Path " << a << " loaded." << std::endl;
	}
	subjresp = true;

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
	startCircle->On();
	startCircle->BorderOn();

	targCircle = new Circle(curtr.xpos, curtr.ypos, TARGET_RADIUS*2, targetColor);
	targCircle->BorderOn();
	targCircle->Off();

	targRotCircle = new Circle(curtr.xpos,curtr.ypos,TARGET_RADIUS*2, targetColor);
	targRotCircle->Off();

	//cursCircle = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS, cursColor);

	returnRing.SetPathColor(cursColor);
	returnRing.SetNVerts(2);
	float ringvert[2][6] = {
							{0, 0, targCircle->Distance(startCircle), 0, PI, 1},
							{0, 0, targCircle->Distance(startCircle), PI, PI, 1}
						   };

	returnRing.SetPathVerts(ringvert);
	returnRing.SetPathWidth(TARGET_RADIUS/2);


	PeakVel = -1;

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
			if (CURSDISCRIM && SHOWHAND)
				cursors[a]= new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*2.5, cursColor);
			else
				cursors[a] = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*1.1, cursColor);
			cursors[a]->BorderOff();
			curs[a] = new HandCursor(cursors[a]); 
			curs[a]->SetOrigin(curtr.startx, curtr.starty);

		}

		//player = new HandCursor(cursors[HAND]); 
		//player->SetOrigin(curtr.startx, curtr.starty);
		player = curs[HAND];  //this is the cursor that represents the hand

	}
	else
	{
		// Use mouse control
		if (CURSDISCRIM && SHOWHAND)
			cursors[0]= new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*2.5, cursColor);
		else
			cursors[0] = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*1.1, cursColor);
		curs[0] = new HandCursor(cursors[0]);
		curs[0]->SetOrigin(curtr.startx, curtr.starty);
		player = curs[0];

	}

	if (CURSDISCRIM && SHOWHAND)
	{
		handCircle = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*1.1, handColor);
		if (SHOWHAND)
			handCircle->On();
		else
			handCircle->Off();

		player->On();

		cursKRCircle = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*1.1, cursColor);
		cursKRCircle->BorderOff();
		cursKRCircle->Off();

		handKRCircle = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*1.75,blkColor);
		handKRCircle->SetBorderColor(handColor);
		handKRCircle->setBorderWidth(0.001f);
		handKRCircle->BorderOn();
		handKRCircle->Off();
	}
	else
	{
		handCircle = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*2.5, handColor);
		if (SHOWHAND)
			handCircle->On();
		else
			handCircle->Off();

		cursKRCircle = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*1.75, blkColor);
		cursKRCircle->SetBorderColor(cursColor);
		cursKRCircle->setBorderWidth(0.001f);
		cursKRCircle->BorderOn();
		cursKRCircle->Off();

		handKRCircle = new Circle(curtr.startx, curtr.starty, CURSOR_RADIUS*1.1, handColor);
		handKRCircle->BorderOff();
		handKRCircle->Off();

	}




	//start = new Object2D(startCirc);
	//start->SetSize(.02f,.02f);
	//start->SetPos(PHYSICAL_WIDTH / 2, PHYSICAL_HEIGHT / 2); // Set the start marker in the center
	//start->On();
	//target = new Object2D(circle);
	//target->SetSize(.04f,.04f);

	hit = new Sound("Resources/coin.wav");
	if (hit == NULL)
		std::cerr << "Sound hit did not load." << std::endl;
	beep = new Sound("Resources/startbeep.wav");
	if (beep == NULL)
		std::cerr << "Sound beep did not load." << std::endl;

	neutralfdbk = new Sound("Resources/mid.wav");
	if (neutralfdbk == NULL)
		std::cerr << "Sound neutralfdbk did not load." << std::endl;

	/* To create text, call a render function from SDL_ttf and use it to create
	 * an Image object. See http://www.libsdl.org/projects/SDL_ttf/docs/SDL_ttf.html#SEC42
	 * for a list of render functions.
	 */
	font = TTF_OpenFont("Resources/arial.ttf", 28);
	scoretext = new Image(TTF_RenderText_Blended(font, "0 points", textColor));
	scorebtext = new Image(TTF_RenderText_Blended(font, "0 bonus points", textColor));
	//text = new Image(TTF_RenderText_Blended(font, "there is text here", textColor));
	//text2 = new Image(TTF_RenderText_Blended(font, "there is text here", textColor));
	trialnumfont = TTF_OpenFont("Resources/arial.ttf", 12);
	trialnum = new Image(TTF_RenderText_Blended(trialnumfont, "1", textColor));
	
/*
	// Create directory with date and time etc
		time_t current_time = time(0);
		tm* ltm = localtime(&current_time);
		std::stringstream path;
		path << "1000";
		/ *
		path << std::setw(4) << std::setfill('0') << ltm->tm_year + 1900;
		path << std::setw(2) << std::setfill('0') << ltm->tm_mon + 1;
		path << std::setw(2) << std::setfill('0') << ltm->tm_mday;
		path << "_";
		path << std::setw(2) << std::setfill('0') << ltm->tm_hour;
		path << std::setw(2) << std::setfill('0') << ltm->tm_min;
		path << std::setw(2) << std::setfill('0') << ltm->tm_sec;
		path << ".txt";* /
		//LPCWSTR tmp = (LPCWSTR) path.str().c_str();
		//LPCSTR tmp = (LPCSTR) path.str().c_str();
		//tmp = "Data5";
	//CreateDirectoryA(tmp, NULL);
	//	CreateDirectory(L"Data2", NULL);
	writer = new DataWriter();


	std::stringstream TFpath;
	TFpath << DATA_PATH << "/tFile.tgt";

	// Check if Data Directory Exists. If not, create it.
	bool dirExist = dirExists(DATA_PATH);
	if(!dirExist){
		mkdir(DATA_PATH);
		std::cerr << "Requested new Data Path to be created. " << std::endl;
	}
	/ *else{
		if(fExists(TFpath.str())){
			return false;
		}
	}* /
	dirExist = dirExists(DATA_PATH);
	if(!dirExist){
		return false;
		std::cerr << "Data Path was not found. " << std::endl;
	}	
	
	tFileCopy.open(TFpath.str(),std::ios::out);
	//tFileCopy << "Testing\n";
	*/

	SDL_WM_SetCaption("Adapt_With_Hand", NULL);


	state = Idle; // Set the initial game state


	std::cerr << std::endl << ">>>Initialization complete.<<<" << std::endl << std::endl;

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


static void setup_opengl()
{
	glClearColor(0, 0, 0, 0);

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

	std::cerr << "Line Width Range: " << size[0] << " - " << size[1] << std::endl;
	std::cerr << "Line Width Incr: " << increment << std::endl;

	//float lineWidth[2];
	//glGetFloatv(GL_LINE_WIDTH_RANGE, lineWidth);

	//glEnable(GL_TEXTURE_2D);
}

void clean_up()
{
	for (int a = 0; a <= (birds_connected ? BIRDCOUNT : 0); a++)
	{
		//delete curs[a];
		delete controller[a];
	}
	//delete player;
	//delete start;
	//delete target;
	delete startCircle;
	delete targCircle;
	delete hit;
	delete beep;
	delete trialnum;
	delete scoretext;
	//delete text;
	//delete text2;
	delete writer;
	//tFileCopy.close();
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

	/* draw trial number on screen
	std::stringstream textss;
	textss << trial.Number();
	delete trialNum;
	trialNum = new Image(TTF_RenderText_Blended(font, textss.str().c_str(), textColor));
	trialNum->Draw(0.8f,0.6f);
	*/


	//draw the trace specified on top of the base trace, centered in the upper portion of the screen
	if (drawstruc.drawtrace >= 0)
	{
		targettraces[drawstruc.drawtrace]->SetPos(curtr.startx,curtr.starty);  //position trace in the upper half of the center screen
		//targettraces[drawstruc.drawtrace]->Draw(275*PHYSICAL_RATIO,275*PHYSICAL_RATIO);
		targettraces[drawstruc.drawtrace]->Draw();
		Target.trace = drawstruc.drawtrace;
	}
	else
		Target.trace = -99;


	//draw the velocity feedback bar
	if (drawstruc.drawvelbar >= 0)
	{
		VelBarFrame->DrawAlign(25*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16,VelBarFrame->GetWidth(),VelBarFrame->GetHeight(),4);
		VelBarWin->DrawAlign(25*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16+(VelBarFrame->GetHeight()/(VELBARMAX-VELBARMIN))*VELMIN,VelBarWin->GetWidth(),(VelBarFrame->GetHeight()/(VELBARMAX-VELBARMIN))*(VELMAX-VELMIN),4);
		VelBar->DrawAlign(25*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16,VelBarFrame->GetWidth()*0.8,VelBarFrame->GetHeight()*drawstruc.drawvelbar,4);	
	}

	/*
	//for testing the criterion radii
	t1Circle->On();
	t1Circle->Draw();
	t3Circle->On();
	t3Circle->Draw();
	*/

	//draw the target circle if the drawState is set to 1
	targCircle->Draw();
	if (targCircle->drawState())
	{
		Target.tgtx = targCircle->GetX();
		Target.tgty = targCircle->GetY();
	}
	else
	{
		Target.tgtx = -99;
		Target.tgty = -99;
	}

	//draw the start circle if the drawState is set to 1
	startCircle->Draw();
	if (startCircle->drawState())
	{
		Target.startx = startCircle->GetX();
		Target.starty = startCircle ->GetY();
	}
	else
	{
		Target.startx = -99;
		Target.starty = -99;
	}


	if (CURSDISCRIM)
	{
		//do discrimination in the cursor

		//draw the feedback cursor, if it is set in the state machine
		handKRCircle->Draw();
		cursKRCircle->Draw();

		//draw the cursor
		player->Draw();

		//draw the hand cursor
		handCircle->Draw();

		if (drawstruc.drawpath >= 0 && DODISCRIM)
			landoltc[drawstruc.drawpath].Draw(player->GetX(),player->GetY());
		Target.path = drawstruc.drawpath;


	}
	else
	{
		//do discrimination in the hand cursor

		//draw the feedback cursor, if it is set in the state machine
		cursKRCircle->Draw();
		handKRCircle->Draw();

		//draw the hand cursor
		handCircle->Draw();

		//draw the cursor
		player->Draw();

		if (drawstruc.drawpath >= 0 && DODISCRIM)
			landoltc[drawstruc.drawpath].Draw(handCircle->GetX(),handCircle->GetY());
		Target.path = drawstruc.drawpath;


	}

	if (drawstruc.drawRing)
	{
		returnRing.SetOneVert(0,2,player->Distance(startCircle));
		returnRing.SetOneVert(1,2,player->Distance(startCircle));
		returnRing.Draw(curtr.startx,curtr.starty);
	}

	//text->Draw(0.2f, 0.2f);
	//text2->Draw(0.2f,0.1f);
	if (drawstruc.drawtext[0] >= 0)
	{
		std::stringstream texts;
		if (updateScore)
		{
			texts << drawstruc.drawtext[0] << " points!"; 
			delete scoretext;
			scoretext = new Image(TTF_RenderText_Blended(font, texts.str().c_str(), textColor));
			updateScore = false;

			if (DODISCRIM)
			{
				texts.str("");
				texts << respscore << " bonus points!"; 
				delete scorebtext;
				scorebtext = new Image(TTF_RenderText_Blended(font, texts.str().c_str(), textColor));
			}

		}
		scoretext->Draw(25*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16-PHYSICAL_HEIGHT*1/32);

		if (DODISCRIM)
			scorebtext->Draw(25*PHYSICAL_WIDTH/32,5*PHYSICAL_HEIGHT/16-PHYSICAL_HEIGHT*2/32);

	}




	//write the trial number
	trialnum->Draw(PHYSICAL_WIDTH*23/24, PHYSICAL_HEIGHT*23/24);

	SDL_GL_SwapBuffers();
	glFlush();
}


float tmpx,tmpy;

void game_update()
{
	switch (state)
	{
		case Idle: // wait for player to start trial by entering start circle

			// configure start target
			startCircle->setBorderWidth(0.001f);

			cursKRCircle->Off();
			handKRCircle->Off();
			didhandfdbk = 0;
			recordhand = false;

			Target.TrType = -1;
			Target.rot = 0;

			drawstruc.drawpath = -1;
			diddiscrim = false;

			if (CurTrial > 0 && (player->Distance(startCircle) > VIS_RADIUS))
			{
				player->SetRotation(0);
				player->Off();
				drawstruc.drawRing = true;
			}
			else
			{
				player->On();
				drawstruc.drawRing = false;
			}

			if (CurTrial == 0)
				drawstruc.drawtrace = -99;
			else
				drawstruc.drawtrace = curtr.trace;

			drawstruc.drawtext[0] = score;
			Target.score = 0;

			if (DODISCRIM && CurTrial>0)
			{
				if (gotvel && ((jsbutton == 9 && trtbl[CurTrial-1].path == 3) || (jsbutton == 10 && trtbl[CurTrial-1].path == 1)) )  //it is really going to be hard for adaptation block if drift... with gotscore, but gotvel might be ok
				//if ( ((jsbutton == 9 && trtbl[CurTrial-1].path == 3) || (jsbutton == 10 && trtbl[CurTrial-1].path == 1)) )
				{
					jsbutton = -1;
					respscore++;
					gotscore = false;
					gotvel = false;
					updateScore = true;
				}
			}


			/*
			//if this is the start of the first trial, keep the velocity variable cleared
			if (CurTrial == 0)
			{
				drawstruc.drawvelbar = -1;
				PeakVel = -1;
			}
			*/

			if (player->Distance(startCircle) < START_RADIUS && (subjresp || !DODISCRIM))
			{ 
				hoverTimer = SDL_GetTicks();

				std::stringstream texttn;
				texttn << CurTrial+1;  //CurTrial starts from 0, so we add 1 for convention.
				delete trialnum;
				trialnum = new Image(TTF_RenderText_Blended(trialnumfont, texttn.str().c_str(), textColor));
				std::cerr << "Trial " << CurTrial+1 << " started at " << SDL_GetTicks() << "." << std::endl;

				/*
				if(!recordingStarted)
				{	// start recording data in a new file (if not started already)
					writer->Close();
					std::stringstream ss;
					if(CurTrial < 10)
					{
						ss << DATA_PATH << "/Trial_00" << CurTrial << "_";
					}
					else if(CurTrial<100)
					{
						ss << DATA_PATH <<"/Trial_0" << CurTrial << "_";
					}
					else
					{
						ss << DATA_PATH <<"/Trial_" << CurTrial << "_";
					}
					writer = new DataWriter(&sysconfig,ss.str().c_str());

					//write out a copy of the current trial into the copied trial table in the data directory.
					tFileCopy << curtr.TrialType << " " 
						      << curtr.startx << " " 
							  << curtr.starty << " "
							  << curtr.xpos << " "
							  << curtr.ypos << " "
							  << curtr.trace << std::endl;

					recordingStarted = true;
				}
				*/
				//std::cerr << "Going to WaitGo State." << std::endl;
				state = WaitGo;
			}
			break;

		case WaitGo: // wait for ITI ms before presenting target + GO cue
			
			drawstruc.drawtrace = curtr.trace;

			if (player->Distance(startCircle) > START_RADIUS)
			{   // go back to Idle if player leaves start circle
				hoverTimer = SDL_GetTicks();
				state = Idle;
			}
			else if (SDL_GetTicks() - hoverTimer >= ITI)
			{

				//if we have hovered long enough, start the trial

				Target.TrType = curtr.TrialType;

				drawstruc.drawvelbar = -1;
				PeakVel = -1;

				if (curtr.path >= 0) //&& curtr.fdbkflag != 0
					drawstruc.drawpath = 0;
				else 
					drawstruc.drawpath = -1;
				diddiscrim = false;

				subjresp = false;
				jsbutton = -1;
				gotscore = false;
				gotvel = false;

				// configure target
				targCircle->SetColor(targetColor);
				targCircle->SetPos(curtr.xpos+curtr.startx, curtr.ypos+curtr.starty);
				targCircle->On();

				RotMat[0] = cos(-curtr.rotation*PI/180);
				RotMat[1] = sin(-curtr.rotation*PI/180);
				RotMat[2] = -sin(-curtr.rotation*PI/180);
				RotMat[3] = cos(-curtr.rotation*PI/180);

				tmpx = RotMat[0]*curtr.xpos + RotMat[1]*curtr.ypos + curtr.startx;
				tmpy = RotMat[2]*curtr.xpos + RotMat[3]*curtr.ypos + curtr.starty;

				targRotCircle->SetPos(tmpx,tmpy);
				
				beep->Play();

				didhandfdbk = 0;
				recordhand = false;

				drawstruc.drawtrace = curtr.trace;

				if (curtr.TrialType == 1)  //use the rotation information to adjust the target
				{
					player->SetRotation(curtr.rotation); // set rotation
					Target.rot = curtr.rotation;
				}
				else if (curtr.TrialType == 2)  //use the rotation information only to adjust the scoring target
				{
					player->SetRotation(0);
					Target.rot = curtr.rotation;
				}

				gameTimer = SDL_GetTicks();
				trialDone = 0;

				std::cerr << "Going to WaitStart State." << std::endl;
				state = WaitStart;
			}
			break;

		case WaitStart:	// wait for subject to start moving
			
			drawstruc.drawtrace = curtr.trace;

			//update peak velocity variable
			if (player->GetVel() > PeakVel)
				PeakVel = player->GetVel();
			drawstruc.drawvelbar = (PeakVel-VELBARMIN)/(2*(VELBARMAX-VELBARMIN));  //draw the velocity feedback bar; the valid region is the lower half of the bar.
			drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0 : drawstruc.drawvelbar);
			drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1 : drawstruc.drawvelbar);				

			if ((player->GetVel() > VEL_THR) || (player->Distance(startCircle) > 2*startCircle->GetRadius()) )
			{

				didhandfdbk = 0;
				recordhand = true;

				movTimer = SDL_GetTicks();
				hoverTimer = SDL_GetTicks();
				targetHit = false;
				reachEnd = false;

				std::cerr << "Going to Reach State." << std::endl;
				state = Reach;
			}
			break;


		case Reach: // wait for subject to finish moving

			drawstruc.drawtrace = curtr.trace;

			//update peak velocity variable
			if (player->GetVel() > PeakVel)
				PeakVel = player->GetVel();
			drawstruc.drawvelbar = (PeakVel-VELBARMIN)/(VELBARMAX-VELBARMIN);  //draw the velocity feedback bar; the valid region is the lower half of the bar.
			drawstruc.drawvelbar = (drawstruc.drawvelbar<0 ? 0 : drawstruc.drawvelbar);
			drawstruc.drawvelbar = (drawstruc.drawvelbar>1 ? 1 : drawstruc.drawvelbar);				

			if (player->Distance(startCircle) > curtr.pathext && (!diddiscrim || SDL_GetTicks()-hoverTimer < PATHDUR))
			{
				if (curtr.path >= 0 && curtr.fdbkflag != 0)
					drawstruc.drawpath = curtr.path;
				
				if (!diddiscrim)
				{
					diddiscrim = true;
					hoverTimer = SDL_GetTicks();
				}
			}
			else
			{
				if (curtr.path >= 0 && curtr.fdbkflag != 0)
					drawstruc.drawpath = 0;
			}

			//note if the subject has exceeded the target radius
			if (!reachEnd && ((player->Distance(startCircle) > (targCircle->Distance(startCircle)+TARGET_RADIUS/4)) || (SDL_GetTicks() - movTimer) > MOV_TIME))
			{

				reachEnd = true;

				cursor_feedback(cursKRCircle,CursPosFdbk);
				cursor_feedback(handKRCircle,HandPosFdbk);
				recordhand = false;
				if (curtr.fdbkflag != 0)
					cursKRCircle->On();

				//if (SHOWHAND)
				//	handKRCircle->On();

				hoverTimer = SDL_GetTicks();

				// note if the subject has hit the target
				if( ((curtr.TrialType==1 && player->HitTarget(targCircle)) || (curtr.TrialType==2 && player->HitTarget(targRotCircle))) && !targetHit)
				{   
					targetHit = true;

					if ((PeakVel < VELMAX) && (PeakVel > VELMIN) && curtr.fdbkflag != 0)
					{
						hit->Play();
						score += 1;
						gotscore = true;
						targCircle->SetColor(hitColor);
					}
					drawstruc.drawtext[0] = score;
					Target.score = score;
					updateScore = true;

				}

				if (curtr.fdbkflag == 0)
					neutralfdbk->Play();


			}

			if ((PeakVel < VELMAX) && (PeakVel > VELMIN))
				gotvel = true;

			
			if (reachEnd && player->Distance(startCircle) > VIS_RADIUS)
			{
				player->SetRotation(0);
				player->Off();
				drawstruc.drawpath = -1;
			}
			else if (curtr.fdbkflag != 0)
			{
				player->On();
			}
			else if (curtr.fdbkflag == 0)
				player->Off();
			

			if ((reachEnd && (SDL_GetTicks()-hoverTimer > 600)) ||  ((SDL_GetTicks() - movTimer) > 2*MOV_TIME) )
			{   // end trial
				trialDone = 1;
				targCircle->Off();

				//drawstruc.drawtrace = -99;

				CurTrial++;


				std::cerr << "Reach state completed at " << SDL_GetTicks() << ". " << std::endl;

				//if we are end of the trial table, quit; otherwise start the next trial
				if(CurTrial>= NTRIALS)  
				{   
					hoverTimer = SDL_GetTicks();
					state = Exiting;
				}
				else
				{
					// reset flags
					targetHit = 0;
					//recordingStarted = false;

					drawstruc.drawRing = true;

					state = Idle;
				}
			}
			break;


		case Exiting:

			player->Null();
			player->ClampOff();
			player->On();
			if (SHOWHAND)
				handCircle->On();
			targCircle->Off();
			cursKRCircle->Off();
			handKRCircle->Off();
			drawstruc.drawtrace = -99;
			drawstruc.drawvelbar = -99;
			drawstruc.drawRing = false;
			Target.score = 0;


			if(SDL_GetTicks()-hoverTimer > 5000)
			{
				quit = true;
			}
			break;
	}
}

/*
bool dirExists(const std::string& dirName_in)
{
  DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
  if (ftyp == INVALID_FILE_ATTRIBUTES)
    return false;  //something is wrong with your path!

  if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
    return true;   // this is a directory!

  return false;    // this is not a directory!
}

bool fExists(const std::string& dirName_in)
{
  DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
  if (ftyp == INVALID_FILE_ATTRIBUTES)
    return false;  //something is wrong with your path!

  return true;
}
*/