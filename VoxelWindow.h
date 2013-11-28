#ifndef VOXELWINDOW_H
#define VOXELWINDOW_H
/********************************/
/* fenetre d'affichage du voxel */
/********************************/

#include <DirectWindow.h>

// defines for screen parameters
// try 320x200, 320x240, they are way faster
#define SCREEN_X_START   100     // the width of the viewing surface
#define SCREEN_Y_START   100     // the height of the viewing surface
#define SCREEN_WIDTH     320     // the width of the viewing surface
#define SCREEN_HEIGHT    240     // the height of the viewing surface
#define SCREEN_BPP       32      // the bits per pixel
#define SCREEN_COLORS    256     // the maximum number of colors

// defines for fixed point math
#define FIXP_SHIFT       12      // number of decimal places 20.12 	
#define FIXP_MUL         4096    // 2^12, used to convert reals

// defines for angles
#define PIE              ((double)(3.14159265)) // take a guess

#define ANGLE_360        (SCREEN_WIDTH * 360/60) // fov is 60 deg, so SCREEN_WIDTH * 360/60
#define ANGLE_180        (ANGLE_360/2)
#define ANGLE_120        (ANGLE_360/3)
#define ANGLE_90		 (ANGLE_360/4)
#define ANGLE_60         (ANGLE_360/6)
#define ANGLE_45         (ANGLE_360/8) 
#define ANGLE_30		 (ANGLE_360/12)
#define ANGLE_20         (ANGLE_360/18) 
#define ANGLE_15         (ANGLE_360/24)
#define ANGLE_10         (ANGLE_360/36)
#define ANGLE_5          (ANGLE_360/72)
#define ANGLE_2          (ANGLE_360/180) 
#define ANGLE_1          (ANGLE_360/360) 
#define ANGLE_0          0
#define ANGLE_HALF_HFOV  ANGLE_30


// defines for height field
#define HFIELD_WIDTH      512	// width of height field data map
#define HFIELD_HEIGHT     512   // height of height field data map  
#define HFIELD_BIT_SHIFT  9     // log base 2 of 512
#define TERRAIN_SCALE_X2  3     // scaling factor for terrain

#define VIEWPLANE_DISTANCE (SCREEN_WIDTH/64)

#define MAX_ALTITUDE     1000   // maximum and minimum altitudes
#define MIN_ALTITUDE     50
#define MAX_SPEED        32     // maximum speed of camera

#define START_X_POS      256    // starting viewpoint position
#define START_Y_POS      256 
#define START_Z_POS      700

#define START_PITCH      80*(SCREEN_HEIGHT/240)    // starting angular heading
#define START_HEADING    ANGLE_90

#define MAX_STEPS        200    // number of steps to cast ray

class BLocker;
class BBitmap;

class VoxelWindow : public BDirectWindow
{
public:
	// variables
	int32			*_bits;
	int32			_rowBytes;
	color_space		_format;
	clipping_rect	_bounds;
	uint8			_sizeByte;
	uint8			_cmptBytes;
	int32			_color;
	
	uint32			_numClippingRect;
	clipping_rect	*_clipList;
	
	bool			_dirty;
	bool			_connected;
	bool			_connectedDisabled;
	BLocker			*_locker;
	thread_id		_threadID;

	// voxel
	int32	_view_pos_x;		// view point x pos
    int32	_view_pos_y;		// view point y pos
	int32	_view_pos_z;		// view point z pos (altitude)
	int32	_view_ang_x;		// pitch 
	int32	_view_ang_y;		// heading, or yaw
	int32	_view_ang_z;		// roll, unused
	int32	_speed;				// speed of run
	
	// c'est pour le fov, c'est l'effet d'affichage
	int32	_dslope; 

	int32	_correctCOS[ANGLE_360];
	int32	_correctSIN[ANGLE_360];

	// images
	BBitmap		*_height_bmp;
	BBitmap		*_map_bmp;

	// pointer sur les bitmaps
	int32		*_heightMap;
	int32		*_colorMap;	

	// fonctions
	VoxelWindow(BRect frame);
	virtual ~VoxelWindow();

	virtual void		MessageReceived(BMessage *message);
	virtual void		DirectConnected(direct_buffer_info *info);
	virtual bool		QuitRequested();

	static	int32		DrawingThreadStub(void *data);
			int32 		DrawingThread();
			void		ComputeMove();
			void		RenderTerrain();
			void		DrawPoint(int32 x,int32 y,int32 *color);

			void		MoveVoxel(BMessage *message);
			void		Init();
			void		BuildTables();
			int32		CorrectCOS(int32 theta);
			int32		CorrectSIN(int32 theta);
};
#endif