#include "VoxelApp.h"
#include "VoxelWindow.h"
#include "stdio.h"

int main(int argc, char **argv) 
{ 
	VoxelApp	beApp;
	
	beApp.Run();

	return 0; 
}

VoxelApp::VoxelApp()
: BApplication("application/x-vnd.CKJ.VoxelApplication")
{
	// verifier que l'on a le DirectWindow
	if(!BDirectWindow::SupportsWindowMode())
	{
		printf("Window Mode not supported !\n");
		return;
	}
	
	// ok on supporte
	VoxelWindow		*voxelWin;
	
	voxelWin = new VoxelWindow(BRect(SCREEN_X_START,SCREEN_Y_START,SCREEN_X_START + SCREEN_WIDTH,SCREEN_Y_START + SCREEN_HEIGHT));
}

VoxelApp::~VoxelApp()
{
}
