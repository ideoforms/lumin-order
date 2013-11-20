/*----------------------------------------------------------------------------
 *
 * LuminOrderApp:
 *   takes a single movie file as its input, iteratively calculates
 *   brightness values for each frame, and outputs a new movie file
 *   with frames ordered by brightness.
 *
 * This code is available under the terms of the MIT license:
 *   <http://www.opensource.org/licenses/mit-license.php>
 *
 * Written by Daniel Jones, 2011
 *   <http://www.erase.net/>
 *
 *--------------------------------------------------------------------------*/

#include "cinder/app/AppBasic.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"
#include "cinder/Surface.h"
#include "cinder/gl/TextureFont.h"
#include "cinder/qtime/QuickTime.h"
#include "cinder/qtime/MovieWriter.h"

#include <sys/time.h>

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

using namespace ci;
using namespace ci::app;
using namespace ci::gl;
using namespace std;


/*----------------------------------------------------------------------------
 * global config.
 *--------------------------------------------------------------------------*/

#define START_FRAME   0
#define ROUND_TO      10e-3
#define OFFSET_FILE   "offsets.txt"
#define OUTPUT_MOVIE  "output.mov"


/*----------------------------------------------------------------------------
 * millitime():
 *   return time value accurate to milliseconds.
 *--------------------------------------------------------------------------*/
double millitime()
{
    struct timeval tv;
    int rv = gettimeofday(&tv, NULL);
    if (rv) return 0.0;
    double time = (double) tv.tv_sec + tv.tv_usec * 0.000001f;
    return time;
}

/*----------------------------------------------------------------------------
 * predict(double elapsed, int index, int total):
 *   return predicted remaining time (in seconds) based on frame index
 *   and elapsed time.
 *--------------------------------------------------------------------------*/
double predict(double elapsed, int index, int total)
{
    double ratio = (double) index / (double) total;
    double ratio_time = (elapsed / ratio) * (1.0 - ratio);
    return ratio_time;
}

/*----------------------------------------------------------------------------
 * data structure for an individual frame index and its brightness.
 *--------------------------------------------------------------------------*/
class FrameData
{
    public:
        FrameData(int index, double brightness) :
            mIndex(index), mBrightness(brightness) {}
        int mIndex;
        double mBrightness;
};

/*----------------------------------------------------------------------------
 * sort frames based on brightness, rounding to the precision given
 * in ROUND_TO.
 *--------------------------------------------------------------------------*/
bool FrameBrightnessSort(const FrameData &a, const FrameData &b)
{
    double roundA = round(a.mBrightness / ROUND_TO) * ROUND_TO;
    double roundB = round(b.mBrightness / ROUND_TO) * ROUND_TO;
    
    if (roundA != roundB)
        return roundA < roundB;
    else
        return a.mIndex < b.mIndex;
}

/*----------------------------------------------------------------------------
 * global application class, extending Cinder's AppBasic.
 *--------------------------------------------------------------------------*/
class LuminOrderApp : public AppBasic
{
    public:
        void  setup();
        void  update();
        void  draw();

        void  keyDown(KeyEvent event);
        void  fileDrop(FileDropEvent event);


        void  loadMovieFile();
        void  loadBrightnessFile();
        void  processAllFrames();
        float processNextFrame();
        void  saveMovie();
        void  gotoNextFrame();
        void  sortMovie();

        Surface8u				mFrameSurface;
        qtime::MovieSurface		mMovie;
        vector <FrameData>		mFrameData;
        int						mFrameIndex;
        bool					mSorted;
        bool					mSaved;
        string                  mMoviePath;
        TextureFontRef          mTextureFont;
        thread                  mThread;
        char                    mInfoText[128];
};


/*--------------------------------------------------------------------------------
 * setup():
 *   app startup. load and sort our file immediately.
 *------------------------------------------------------------------------------*/
void LuminOrderApp::setup()
{
    mSorted = false;
    mSaved = false;
    sprintf(mInfoText, "Drag a movie file here");
    
	// mMoviePath = getOpenFilePath().string();
    Font mFont = Font("Helvetica Bold", 13);
    mTextureFont = gl::TextureFont::create(mFont);
    
	if (!mMoviePath.empty())
    {
		// loadMovieFile();
        // thread(&LuminOrderApp::loadMovieFile, this);
    }
}

/*----------------------------------------------------------------------------
 * keyDown(KeyEvent event):
 *   handle key events.
 *--------------------------------------------------------------------------*/
void LuminOrderApp::keyDown(KeyEvent event)
{
    char c = event.getChar();
    
    switch (c)
    {
        case 'f':
            setFullScreen(!isFullScreen());
            break;
	}
}

/*----------------------------------------------------------------------------
 * loadMovieFile(const string &moviePath):
 *   read, sort and store the given movie file.
 *--------------------------------------------------------------------------*/
void LuminOrderApp::loadMovieFile()
{
	try
	{
        sprintf(mInfoText, "Opening file...\n");
        
        // load up the movie, set it to loop, and begin playing
		mMovie = qtime::MovieSurface(mMoviePath);
		mMovie.play();
		mMovie.setRate(0);
        
        console() << "loaded file, total " << mMovie.getNumFrames() << " frames" << std::endl;
        
        console() << "processing frames..." << std::endl;
		this->processAllFrames();
        console() << "sorting frames..." << std::endl;
		this->sortMovie();
        console() << "saving output..." << std::endl;
		this->saveMovie();
        console() << "done." << std::endl;
	}
	catch(...)
	{
		console() << "error during encoding." << std::endl;
		mMovie.reset();
	}
}

/*----------------------------------------------------------------------------
 * fileDrop(FileDropEvent event):
 *   alternatively, handle a file via a drag operation.
 *   (only applicable if the 'open' dialog is cancelled)
 *--------------------------------------------------------------------------*/
void LuminOrderApp::fileDrop(FileDropEvent event)
{
    mMoviePath = event.getFile(0).string();
	// loadMovieFile();
    mThread = thread(&LuminOrderApp::loadMovieFile, this);
}

/*----------------------------------------------------------------------------
 * update():
 *   before displaying each frame, check whether the movie has been sorted.
 *--------------------------------------------------------------------------*/
void LuminOrderApp::update()
{
//	if (!mMovie)
//		return;
	
//	if (mSorted)
//		this->gotoNextFrame();
}


/*----------------------------------------------------------------------------
 * processAllFrames():
 *   iterate through every frame of the movie and calculate mean
 *   brightness values, storing in the mFrameData vector.
 *--------------------------------------------------------------------------*/
void LuminOrderApp::processAllFrames()
{
    // if we want to start at a particular frame
    mMovie.seekToFrame(START_FRAME);
    mFrameIndex = START_FRAME;

    double t0 = millitime();
    
	for (int i = 0; i < mMovie.getNumFrames() - START_FRAME; i++)
	{
		double  brightness = this->processNextFrame();
        double  elapsed    = millitime() - t0;
        double  predicted  = predict(elapsed, i, mMovie.getNumFrames());
        int     minutes    = (int) predicted / 60;

        sprintf(mInfoText, "Processing: Frame %d/%d (brightness %.8f, elapsed %.2f, remaining %dm%05.2fs)\n",
                mFrameIndex, mMovie.getNumFrames(), brightness, elapsed, minutes,
                predicted - (60 * minutes));

        if (i % 50 == 0)
            printf("%s", mInfoText);
	}
}

/*----------------------------------------------------------------------------
 * processNextFrame():
 *   iterate through next frame of the movie.
 *--------------------------------------------------------------------------*/
float LuminOrderApp::processNextFrame()
{
	/*----------------------------------------------------------------------------
     * pull out surface object and calculate mean brightness.
     *--------------------------------------------------------------------------*/
    mFrameSurface = mMovie.getSurface();    
    Surface::Iter iter = mFrameSurface.getIter();
    double brightness = 0.0;

    while(iter.line())
    {
        while(iter.pixel())
        {
            brightness += (double) (iter.r() + iter.g() + iter.b()) / (3.0 * 255.0);
        }
    }
    
    brightness /= mFrameSurface.getWidth() * mFrameSurface.getHeight();

    /*----------------------------------------------------------------------------
     * create and store FrameData object.
     *--------------------------------------------------------------------------*/
    FrameData data(mFrameIndex, brightness);
    mFrameData.push_back(data);

    /*----------------------------------------------------------------------------
     * now actually step forward.
     *--------------------------------------------------------------------------*/
    mFrameIndex++;
    mMovie.stepForward();
    
    return brightness;
}

/*----------------------------------------------------------------------------
 * loadBrightnessFile():
 *   import brightnesses and ordering from a previous offsets.txt.
 *--------------------------------------------------------------------------*/
void LuminOrderApp::loadBrightnessFile()
{
    string dirname = fs::path(mMoviePath).parent_path().string();
    string offset_path = dirname + "/" + OFFSET_FILE;
    FILE *fd = fopen(offset_path.c_str(), "r");
    
	int offset;
	float brightness;

	while (fscanf(fd, "%d,%f", &offset, &brightness) != EOF)
	{
		printf("load: [%d] %f\n", offset, brightness);
        mFrameData.push_back(FrameData(offset, brightness));
	}
	fclose(fd);
}

/*----------------------------------------------------------------------------
 * sortMovie():
 *   reorder mFrameData based on brightness values, and store to
 *   offsets.txt.
 *--------------------------------------------------------------------------*/
void LuminOrderApp::sortMovie()
{
    std::sort(mFrameData.begin(), mFrameData.end(), FrameBrightnessSort);
	mSorted = true;
	mFrameIndex = 0;
	
	int n = 0;
    string dirname = fs::path(mMoviePath).parent_path().string();
    string offset_path = dirname + "/" + OFFSET_FILE;
    FILE *fd = fopen(offset_path.c_str(), "w");
    
	for (vector<FrameData>::const_iterator iter = mFrameData.begin();
	     iter != mFrameData.end(); iter++)
	{
        sprintf(mInfoText, "Ordering: Frame %d (index %d, brightness %f)\n", n, iter->mIndex, iter->mBrightness);
        
        if (n % 10 == 0)
            printf("%s", mInfoText);
        n++;
        
        fprintf(fd, "%d,%f\n", iter->mIndex, iter->mBrightness);
	}
    fclose(fd);
}

/*----------------------------------------------------------------------------
 * saveMovie():
 *   use Cinder MovieWriter to output new movie file based on ordering.
 *--------------------------------------------------------------------------*/
void LuminOrderApp::saveMovie()
{
	if (!mSorted)
		return;
	
	qtime::MovieWriter::Format format;
	format.setCodec(qtime::MovieWriter::CODEC_H264);
	format.setQuality(1.0f);
    format.setDefaultDuration(1.0 / mMovie.getFramerate());
	
	printf("saving movie to file %s (width = %d, height = %d, framerate = %f)\n",
	       OUTPUT_MOVIE, mMovie.getWidth(), mMovie.getHeight(), mMovie.getFramerate());
    
    string dirname = fs::path(mMoviePath).parent_path().string();
	qtime::MovieWriter mMovieWriter(dirname + "/output.mov", mMovie.getWidth(),
	                                mMovie.getHeight(), format);

    double t0 = millitime();
    
	long int n = 0;
    printf("iterating frameData\n");
	for (vector<FrameData>::const_iterator iter = mFrameData.begin(); iter != mFrameData.end(); iter++)
	{
        double  elapsed = millitime() - t0;
        double  predicted = predict(elapsed, n, mFrameData.size());
        int     minutes = (int) predicted / 60;
        float   brightness = iter->mBrightness;

        sprintf(mInfoText, "Saving: Frame %ld/%ld (index %d, brightness %.8f, elapsed %.2f, remaining %dm%05.2fs)\n",
                n, mFrameData.size(), iter->mIndex, brightness, elapsed, minutes,
                predicted - 60 * minutes);

        if (n++ % 50 == 0)
        {
            printf("%s", mInfoText);
        }
		       
		mMovie.seekToFrame(iter->mIndex);
		mFrameSurface = mMovie.getSurface();
		mMovieWriter.addFrame(mFrameSurface);
	}
	
	mMovieWriter.finish();
    mSaved = true;
    sprintf(mInfoText, "Export complete.\n");
    printf("%s", mInfoText);
}

/*----------------------------------------------------------------------------
 * gotoNextFrame():
 *   during playback phase, jump to next frame.
 *--------------------------------------------------------------------------*/
void LuminOrderApp::gotoNextFrame()
{
	if (mFrameIndex < mFrameData.size())
	{
		mMovie.seekToFrame(mFrameData[mFrameIndex].mIndex);
		mFrameSurface = mMovie.getSurface();
		mFrameIndex++;
	}
}

/*----------------------------------------------------------------------------
 * draw():
 *   during playback phase, output current frame.
 *--------------------------------------------------------------------------*/
void LuminOrderApp::draw()
{
	gl::clear(Color(0, 0, 0));
	gl::enableAlphaBlending();

	if (mFrameSurface && !mSaved)
	{
		Rectf rect = Rectf(mFrameSurface.getBounds()).getCenteredFit(getWindowBounds(), true);
		gl::color(Color(1, 1, 1));
		gl::draw(mFrameSurface, rect);
	}
    
    gl::color(Color(1, 1, 1));

    mTextureFont->drawString(mInfoText, Vec2f(10, 20));
}


/*----------------------------------------------------------------------------
 * create and start the app.
 *--------------------------------------------------------------------------*/
CINDER_APP_BASIC(LuminOrderApp, RendererGl);
