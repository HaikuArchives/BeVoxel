#include "VoxelWindow.h"
#include <Locker.h>
#include <Application.h>
#include <string.h>
#include <stdlib.h>
#include <Bitmap.h>
#include <TranslationUtils.h>
#include <math.h>
#include <time.h>

#include <stdio.h>

/**** constructeur ****/
VoxelWindow::VoxelWindow(BRect frame)
: BDirectWindow(frame,"VoxelWindow",B_TITLED_WINDOW,B_NOT_RESIZABLE|B_NOT_ZOOMABLE)
{
	_connected = false;
	_connectedDisabled = false;
	_locker = new BLocker();
	_clipList = NULL;
	_numClippingRect = 0;
	
	// chargement image
	Init();
	BuildTables();
	
	_dirty = true;
	_threadID = spawn_thread(DrawingThreadStub,"drawing_thread",B_NORMAL_PRIORITY,(void *)this);
	resume_thread(_threadID);

	// afficher
	Show();
}

/**** destructeur ****/
VoxelWindow::~VoxelWindow()
{
	int32	result;

	_connectedDisabled = true;
	Hide();
	Sync();
	wait_for_thread(_threadID,&result);
	free(_clipList);
	delete _locker;

	// liberer memoire image
	delete _height_bmp;
	delete _map_bmp;
}

/**** charger les images ****/
void VoxelWindow::Init()
{
	_view_pos_x = START_X_POS;		// view point x pos
    _view_pos_y = START_Y_POS;		// view point y pos
	_view_pos_z = START_Z_POS;		// view point z pos (altitude)
	_view_ang_x = START_PITCH;		// pitch 
	_view_ang_y = START_HEADING;	// heading, or yaw
	_view_ang_z = 0;				// roll, unused
	_speed = 4;						// vitesse de depart
	
	// c'est pour le fov, c'est l'effet d'affichage
	_dslope = (int32)(((double)1/(double)VIEWPLANE_DISTANCE)*FIXP_MUL); 

	_height_bmp = BTranslationUtils::GetBitmapFile("./elevation.bmp");
	_map_bmp = BTranslationUtils::GetBitmapFile("./map.bmp");

	// pointer sur les images
	_heightMap = (int32 *)(_height_bmp->Bits());
	_colorMap = (int32 *)(_map_bmp->Bits());
}

/**** creer les tables COS et SIN ****/
void VoxelWindow::BuildTables()
{
	for(int32 curr_angle=0; curr_angle < ANGLE_360; curr_angle++)
	{
		double angle_rad = 2*PIE*(double)curr_angle/(double)ANGLE_360;
	
		// compute sin and cos and convert to fixed point
		_correctCOS[curr_angle] = (int32)(cos(angle_rad) * FIXP_MUL);
		_correctSIN[curr_angle] = (int32)(sin(angle_rad) * FIXP_MUL);
	}
}

/**** gestion des messages ****/
void VoxelWindow::MessageReceived(BMessage *message)
{
	switch(message->what)
	{
		// traiter le clavier
		case '_KYD':
			message = Looper()->DetachCurrentMessage();
			MoveVoxel(message);
			delete message;
			break;
		default:
			BDirectWindow::MessageReceived(message);
	}
}

/**** bouger le voxel ****/
void VoxelWindow::MoveVoxel(BMessage *message)
{
	int8	byte;
	
	// trouver la touche
	if(message->FindInt8("byte",&byte)!=B_OK)
		return;

	// ca doit etre une touche de direction
	switch(byte)
	{
		case 28:	// gauche
			_view_ang_y += ANGLE_5; 
			break;
		case 29:	// droite
			_view_ang_y -= ANGLE_5; 
			break;
		case 30:	// haut
			_speed+=2;
			break;
		case 31:	// bas
			_speed-=2;
			break;
	}
}

/**** connection a la fenetre ****/
void VoxelWindow::DirectConnected(direct_buffer_info *info)
{
	if(!_connected && _connectedDisabled)
		return;

	_locker->Lock();	
	switch(info->buffer_state & B_DIRECT_MODE_MASK)
	{
	case B_DIRECT_START:
		_connected = true;
	case B_DIRECT_MODIFY:
		// recuperer la position de la fenetre
		if(_clipList)
		{
			free(_clipList);
			_clipList = NULL;
		}
		_numClippingRect = info->clip_list_count;
		_clipList = (clipping_rect *)malloc(_numClippingRect*sizeof(clipping_rect));
		if(_clipList)
		{
			memcpy(_clipList,info->clip_list,_numClippingRect*sizeof(clipping_rect));
			_bits = (int32 *)(info->bits);
			_rowBytes = info->bytes_per_row;
			_format = info->pixel_format;
			// definir la taille d'un pixel
			_sizeByte = 1;
			if(_format == B_RGB16 || _format == B_RGB15)
				_sizeByte = 2;
			if(_format == B_RGB32 || _format == B_RGBA32 || _format == B_RGB24)
				_sizeByte = 4;
			
			_bounds = info->window_bounds;
			_dirty = true;
			//info
			printf("bytes_per_row : %ld\n",(int32)_rowBytes);
			printf("format : %ld\n",(int32)_format);
			printf("sizeByte : %ld\n",(int32)_sizeByte);
		}
		break;
	case B_DIRECT_STOP:
		_connected = false;
		break;
	}
	_locker->Unlock();	
}

/**** fonction pour avoir une methode membre ****/
int32 VoxelWindow::DrawingThreadStub(void *data)
{
	VoxelWindow	*voxelWin = (VoxelWindow *)data;
	return voxelWin->DrawingThread();
}

/**** fonction de dessin ****/
int32 VoxelWindow::DrawingThread()
{
	bigtime_t	FPStime;

	while(!_connectedDisabled)
	{
		// on veux au mini 16 ms.
		FPStime = system_time() + 50000;

		_locker->Lock();
		if(_connected)
		{
			if(_dirty)
			{
				ComputeMove();
				RenderTerrain();
			}
		}
		_locker->Unlock();

		// on atend le temps qui reste
		// si il reste du temps a attendre
		FPStime -= system_time();
		if(FPStime > 0)
			snooze(FPStime);
	}
	return B_OK;
}

bool VoxelWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}

/**** fonction de conversion d'angle ****/
// il faut toujours avoir un angle correct
int32 VoxelWindow::CorrectCOS(int32 theta)
{
	if (theta<0)
		return(_correctCOS[theta + ANGLE_360]);
	else
		if(theta>=ANGLE_360)
			return(_correctCOS[theta - ANGLE_360]);
		else
			return(_correctCOS[theta]);
}

// la meme coté cosinus
int32 VoxelWindow::CorrectSIN(int32 theta)
{
	if (theta<0)
		return(_correctSIN[theta + ANGLE_360]);
	else
		if (theta>=ANGLE_360)
			return(_correctSIN[theta - ANGLE_360]);
		else
			return(_correctSIN[theta]);
}

void VoxelWindow::ComputeMove()
{
	// test heading
	if(_view_ang_y > ANGLE_360) 
		_view_ang_y-=ANGLE_360;
	else
		if(_view_ang_y <= ANGLE_0) 
			_view_ang_y+=ANGLE_360;

	// move viewpoint
	_view_pos_x += (_speed * CorrectCOS(_view_ang_y));// >> FIXP_SHIFT);
	_view_pos_y += (_speed * CorrectSIN(_view_ang_y));// >> FIXP_SHIFT);

	// keep viewpoint in playfield
	if(_view_pos_x >=(HFIELD_WIDTH << FIXP_SHIFT)) 
		_view_pos_x = 0;
	else
		if(_view_pos_x < 0)	
			_view_pos_x = ((HFIELD_WIDTH-1)<< FIXP_SHIFT);

	if(_view_pos_y >=(HFIELD_HEIGHT << FIXP_SHIFT))	
		_view_pos_y = 0;
	else
		if(_view_pos_y < 0)	
			_view_pos_y = ((HFIELD_HEIGHT-1)<< FIXP_SHIFT);

	// test speed
	if(_speed > MAX_SPEED) 
		_speed = MAX_SPEED;
	else
		if(_speed < -MAX_SPEED) 
			_speed = -MAX_SPEED;

	// test altitude
	if((_view_pos_z+=8) > MAX_ALTITUDE) 
		_view_pos_z = MAX_ALTITUDE;
	else
		if((_view_pos_z-=8) < MIN_ALTITUDE) 
			_view_pos_z = MIN_ALTITUDE;

}

/**** fonction de rendering du terrain ****/
void VoxelWindow::RenderTerrain()
{
	int32		yDraw;				// pour ce placer sur la bonne ligne
	int32		xr,yr;				// used to compute the point the ray intersects the height data
	int32		currentColumn;		// current column
	int32		currentVoxelScale;	// current scaling factor to draw each voxel line
	int32		currentRow;			// number of rows processed in current column
	int32		currentStep;		// current step ray is at
	int32		raycastAngle;		// current angle of ray being cast
	int32		x_ray,y_ray,z_ray;  // the position of the tip of the ray
	int32		dx,dy,dz;           // general deltas for ray to move from pt to pt
	int32		columnHeight;		// height of the column intersected and being rendered
	int32		mapAddr;			// temp var used to hold the addr of data bytes
	int32		color;				// color of pixel being rendered
	

	// compute starting angle, at current angle plus half field of view
	raycastAngle = _view_ang_y + ANGLE_HALF_HFOV;

	// cast a ray for each column of the screen
	for(currentColumn = 0; currentColumn <= SCREEN_WIDTH; currentColumn++)
	{
		// seed starting point for cast
		x_ray = _view_pos_x;
		y_ray = _view_pos_y;
		z_ray = (_view_pos_z << FIXP_SHIFT);
		
		// compute deltas to project ray at, note the spherical cancelation factor
		dx = CorrectCOS(raycastAngle) << 1;
		dy = CorrectSIN(raycastAngle) << 1;

		// dz is a bit complex, remember dz is the slope of the ray we are casting
		// therefore, we need to take into consideration the down angle, or
		// x axis angle, the more we are looking down the larger the intial dz
		// must be
		dz = _dslope * (_view_ang_x - SCREEN_HEIGHT);

		// reset current voxel scale 
		currentVoxelScale = 0;

		// reset row
		currentRow = 0;

		// get starting address of bottom of current video column 
		yDraw = SCREEN_HEIGHT; 
		// enter into casting loop
		for(currentStep = 0; currentStep < MAX_STEPS; currentStep++)
		{
			// compute pixel in height map to process
			// note that the ray is converted back to an int
			// and it is clipped to to stay positive and in range
			xr = (x_ray  >> FIXP_SHIFT);
			yr = (y_ray  >> FIXP_SHIFT);

			xr = (xr & (HFIELD_WIDTH-1));
			yr = (yr & (HFIELD_HEIGHT-1));

			mapAddr = (xr + (yr << HFIELD_BIT_SHIFT));

			// get current height in height map, note the conversion to fixed point
			// and the added multiplication factor used to scale the mountains
			// on a besoin d'une seule composante qui varie entre 0 et 255
			columnHeight = (int8)(_heightMap[mapAddr]) << (FIXP_SHIFT + TERRAIN_SCALE_X2);

			// test if column height is greater than current voxel height for current step
			// from intial projection point
			if(columnHeight >= z_ray)
			{
				// we know that we have intersected a voxel column, therefore we must
				// render it until we have drawn enough pixels on the display such that
				// thier projection would be correct for the height of this voxel column
				// or until we have reached the top of the screen

				// get the color for the voxel
				color = (int32)(_colorMap[mapAddr]);

				// draw vertical column voxel
				// exit if we can break out of the loop
				while(z_ray <= columnHeight)
				{
					DrawPoint(currentColumn,yDraw,&color);

					// now translate the current z position of the ray by the current voxel
					// scale per unit
					z_ray += currentVoxelScale;

					// now we need to push the ray upward on z axis, so increment the slope
					dz += _dslope;

					// move up one video line
					yDraw--;

					// test if we are done with column
					if(yDraw<0)
					{
						// force exit of outer steping loop
						// chezzy, but better than GOTO!
						currentStep = MAX_STEPS;
						break;
					}
				}
			}
			// update the position of the ray
			x_ray += dx;
			y_ray += dy;
			z_ray += dz;

			// update the current voxel scale, remember each step out means the scale increases
			// by the delta scale
			currentVoxelScale += _dslope;
		}
		
		// complete background
		color = 8192 - yDraw -1;
		for(;color<8192;color++)
		{
			DrawPoint(currentColumn,yDraw,&color);
			yDraw--;
		}
				
		raycastAngle--;	
	}
}

/**** dessin du point ****/
void VoxelWindow::DrawPoint(int32 x,int32 y,int32 *color)
{
	// recalculer la couleur
	switch(_format)
	{
	case B_RGB15:
		// same as * 32 / 256
		_color = *((uint8 *)color) >> 3;
		// same as  * 32 / 256 * 32
		_color += *((uint8 *)color + 1) >> 3 << 5;
		// same as * 32 / 256 * 32 * 32
		_color += *((uint8 *)color + 2) >> 3 << 10;
		break;
	case B_RGB16:
		// same as * 32 / 256
		_color = *((uint8 *)color) >> 3;
		// same as  * 64 / 256 * 32
		_color += *((uint8 *)color + 1) >> 2 << 5;
		// same as * 32 / 256 * 32 * 64
		_color += *((uint8 *)color + 2) >> 3 << 11;
		break;
	default:
		// normalement B_RGB32
		_color = *color;
	}

	// rescaler au bord de la fenetre
	x += _bounds.left;
	y += _bounds.top;
	
	// verifier qu'on est bien dans le clipping
//	for(uint32 i = 0; i < _numClippingRect; i++)
//		if((x >= _clipList[i].left && x <= _clipList[i].right) && (y >= _clipList[i].top && y <= _clipList[i].bottom))
//			memcpy(((uint8 *)(_bits) + x*_sizeByte + _rowBytes*y ),&_color,_sizeByte);

	// sans memcpy
	for(uint8 i=0;i<_sizeByte;i++)
		*((uint8 *)(_bits) + x*_sizeByte + _rowBytes*y + i) = *((uint8 *)&_color + i);
}
