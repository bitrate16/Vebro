// Vebro.cpp : Defines the entry point for the application.
//
// Usable: https://zetcode.com/gui/winapi/menus/

#include "framework.h"
#include "Vebro.h"

// Debug definisions
// #define DEBUG_DISPLAY_CONSOLE_WINDOW

// Main Tray menu
// Base disabled items id (Items used only for text display with no function)
#define TRAY_MENU_BASE_DISABLED_ID 30500
// Base menu ID
#define TRAY_MENU_BASE_ID 40001

// Separator
#define IDM_SEP 40000


// Globals
// >> Tray related
NOTIFYICONDATA trayNID;
HWND           trayWindow;
HICON          trayIcon;

// Tray menu object
HMENU trayMainMenu = 0;

// handlers for functional buttons
std::vector<std::function<void()>> trayMenuHandlers;


// >> Displays related
RECT              fullViewportSize; // Size of displays in total
std::vector<RECT> rawDisplays;      // List of existing displays' dimensions as raw relative sizes
std::vector<RECT> displays;         // List of existing displays' dimensions


// >> Wallpaper related
RECT     currentWindowDimensions;
HWND     glWindow;
HPALETTE glPalette;
HDC      glDevice;
HGLRC    glContext;
int      glWidth;
int      glHeight;
BOOL     wndDebugOutput = FALSE;


// >> OpenGL related
// Buffers for viewport coords
GLuint glSquareVAO;
GLuint glSquareVBO;
GLuint glSquareEBO;

// Shaders (if exists)
// Shader for texture copy
GLuint glPassthroughShaderProgramID;

// Main shader
GLuint glMainShaderProgramID = -1;   // Main shader program ID
std::wstring glMainShaderPath = L""; // Path to the main shader (For support reload button)

// Value == -1 indicates that shader sould not be rendered
GLuint glBufferShaderProgramIDs[4] = { -1, -1, -1, -1 };     // Buffer i shader program (A / B / C / D)
std::wstring glBufferShaderPath[4] = { L"", L"", L"", L"" }; // Path to the Buffer i shader (For support reload button)
int scBufferFrames[4] = { 0, 0, 0, 0 };                      // Frame number for each buffer shader (fictional, used only to prevent flickering and correctly save frame number on unload)

// Framebuffers for these shaders
// 2 Framebuffers for each single buffer to enable multipass
GLuint glBufferShaderFramebuffers[2][4];

// Textures to render in.
//  This textures will exist till the end of the program because they 
//   are used as temp variables storing buffer output for the next frame 
//   and can be used as input even if shader for this buffer (A / B / C / D) 
//   was removed.
// 2 Textures for each single buffer to enable multipass
GLuint glBufferShaderFramebufferTextures[2][4];

// Indicates if buffer[i] should be rendered
// Changed view resource control
// Render requires (glBufferShaderShouldBeRendered[i] && glBufferShaderProgramIDs[i] != -1)
//  to be true in order to render the shader because shader can be 
//  loaded but not rendered (in this case it is only compiled and not used) or 
//  rendered but not loaded (in this case buffer texture of shader is rendered, but code is not compiled).
// Shader code can be unloaded by Remove button in menu.
// Shader as input can be disabled by removing it manually from inputs.
BOOL glBufferShaderShouldBeRendered[4] = { FALSE, FALSE, FALSE, FALSE };


// >> Scene related
BOOL   scFullscreen     = FALSE;       // Indicates if scene is fullscreen (Full desktop space)
int    scDisplayID      = 0;           // Contains ID of the display that is currently used as render display 
BOOL   scPaused         = FALSE;       // Indicates if rendering is paused
double scPauseTimestamp = 0;           // Holds timestamp of the pause
double scTimestamp      = 0;           // Holds timestamp of the previous frame
int    scFrames         = 0;           // Holds number of frames rendered
BOOL   scMouseEnabled   = FALSE;       // Indicates if Mouse input is enabled
POINT  scMouse = { 0, 0 };             // Holds last mouse location
int    scMinFrameTime   = 1000 / 30;   // Holds minimal frame time in ms (1000 / FPS)
int    scFPSMode        = 30;          // Just defines the FPS used
BOOL   scSoundEnabled   = FALSE;       // Indicates if sound capture enabled / disabled. Used to force disable sound capture if shader uses audio input
std::wstring scPackPath = L"";         // Defines full path for pack locations

// ID's for all shader inputs
// Should only be changed via special functions to correctly process GC
int scMainShaderInputs[4] = { -1, -1, -1, -1 };
int scBufferShaderInputs[4][4] = { { -1, -1, -1, -1 }, { -1, -1, -1, -1 }, { -1, -1, -1, -1 }, { -1, -1, -1, -1 } };


// >> Threading related
std::thread* renderThread = nullptr;   // Thread for rendering the wallpaper
std::mutex   renderMutex;              // Captured on each draw frame
BOOL         appExiting = FALSE;       // Indicates if application exits
BOOL         appLockRequested = FALSE; // indicates if main thread wants to acquire lock


// >> Resources related
// Resource type
// Buffer : id (A / B / C / D)
// Image : path, GLuint bind (image texture)
// Video : path, GLuint bind (current frame as texture), specific media data
// Audio : path, GLuint bind (current FFT as texture), specific media data
// Webcam : GLuint bind (current frame as texture)
// Microphone : GLuint (current FFT as texture)
// TODO: Cubemap input
enum ResourceType {
	IMAGE_TEXTURE,    // Simple image as input
	AUDIO_TEXTURE,    // Audio file as input
	VIDEO_TEXTURE,    // Video file as input
	MIC_TEXTURE,      // Microphone as inout
	WEB_TEXTURE,      // Webcam as input
	KEYBOARD_TEXTURE, // Keyboard key states as input
	FRAME_BUFFER      // Buffer A / B / C / D as input
};

// Resource unit
struct SCResource {

	// Resource references amount (auto GC)
	int refs = 0;

	// Resource type
	ResourceType type;

	// Bind for texture (id exists for this unit)
	GLuint bind = 0;

	// ID number of buffer (only for buffer input)
	int buffer_id = 0;

	// Absolute path of resource (used to determine duplications)
	std::wstring path;

	// Dimensions
	int width;
	int height;
};

// Returns short description of the resource state
std::wstring resourceToShordDescription(SCResource& res) {
	switch (res.type) {
		case IMAGE_TEXTURE: return std::wstring(L"Image [") + std::wstring(res.path.begin(), res.path.end()) + L"]";
		case AUDIO_TEXTURE: return L"Audio";
		case VIDEO_TEXTURE: return L"Video";
		case MIC_TEXTURE: return L"Microphone";
		case WEB_TEXTURE: return L"Webcam";
		case KEYBOARD_TEXTURE: return L"Keyboard";
		case FRAME_BUFFER: return std::wstring(L"Buffer ") + L"ABCD"[res.buffer_id];
		default: return L"";
	}
}

// Resource storage table (used to prevent reduplications of loading and processing)
// Storas static fixed amount of resources matching the total amount of inputs for all shaders
// Allows deletion of any element and insertion into any free cell
struct ResourceTableEntry {
	BOOL empty = TRUE;
	SCResource resource;
};

// 4 main inputs + 4 buffers * 4 inputs
const int ResourceTableSize = 4 + 4 * 4;

ResourceTableEntry scResources[ResourceTableSize];

// Finds the specified resource and returns it's ID on success
int findResource(SCResource res) {
	for (int i = 0; i < ResourceTableSize; ++i) {
		if (scResources[i].empty)
			continue;

		if (res.type == scResources[i].resource.type) {
			switch (res.type) {
				case IMAGE_TEXTURE: {
					if (res.path == scResources[i].resource.path)
						return i;
					return -1;
				}

				case AUDIO_TEXTURE: {
					if (res.path == scResources[i].resource.path)
						return i;
					return -1;
				}

				case VIDEO_TEXTURE: {
					if (res.path == scResources[i].resource.path)
						return i;
					return -1;
				}

				case MIC_TEXTURE:
				case WEB_TEXTURE: {
					return -1;
				}

				case FRAME_BUFFER: {
					if (res.buffer_id == scResources[i].resource.buffer_id)
						return i;
					return -1;
				}
			}
		}
	}

	return -1;
}

// Load and initialize the desired resource
// Returns ResourceID as index in scResources or -1
int loadResource(SCResource res) {

	int resID = findResource(res);

	if (resID != -1) {
		++scResources[resID].resource.refs;
		return resID;
	}

	// Called once when initially loading resource:
	switch (res.type) {
		case IMAGE_TEXTURE: {
			res.refs = 1;

			std::vector<unsigned char> image;
			unsigned width, height;

			// Convert path to char*
			// Google says that MAX_PATH is 260, but i'm not sure, lol
			// <strike>
			// idfk why it is STILL can not be done in one line
			// using convert_type = std::codecvt_utf8<wchar_t>;
			// std::wstring_convert<convert_type, wchar_t> converter;
			// std::string stringPath = converter.to_bytes(res.path);
			// </strike>
			// Updated after 15 seconds:
			// https://stackoverflow.com/a/18374698
			std::string stringPath = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(res.path);

			unsigned error = lodepng::decode(image, width, height, stringPath);

			std::wcout << "Loading resource for Texture [" << res.path << "] (" << width << ", " << height << ')' << std::endl;

			if (error != 0) {

				std::wcout << "Image resource load error: " << lodepng_error_text(error) << std::endl;
				MessageBoxA(
					NULL,
					lodepng_error_text(error),
					"Image load error",
					MB_ICONERROR | MB_OK
				);

				return -1;
			}

			// Flip image vertically
			for (size_t y = 0; y < height / 2; y++)
				for (size_t x = 0; x < width; x++)
					for (size_t c = 0; c < 4; c++)
						std::swap(image[4 * width * y + 4 * x + c], image[4 * width * (height - y - 1) + 4 * x + c]);

			// Find closest power of two
			size_t u2 = 1; while (u2 < width) u2 *= 2;
			size_t v2 = 1; while (v2 < height) v2 *= 2;

			res.width = u2;
			res.height = v2;

			if (u2 != width)
				std::wcout << "Texture warning: width must be power of two, got " << width << ", resizing to closest " << u2 << std::endl;
			if (v2 != height)
				std::wcout << "Texture warning: height must be power of two, got " << height << ", resizing to closest " << v2 << std::endl;
			
			// To size of two conversion ratio
			double u3 = (double) width / u2;
			double v3 = (double) height / v2;

			// Resize texture to power of two
			std::vector<unsigned char> image2(u2 * v2 * 4);
			for (size_t y = 0; y < height; y++)
				for (size_t x = 0; x < width; x++)
					for (size_t c = 0; c < 4; c++) 
						image2[4 * u2 * y + 4 * x + c] = image[4 * width * y + 4 * x + c];

			// Generate texture & pass pixeldata
			glGenTextures(1, &res.bind);
			glBindTexture(GL_TEXTURE_2D, res.bind);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexImage2D(GL_TEXTURE_2D, 0, 4, u2, v2, 0, GL_RGBA, GL_UNSIGNED_BYTE, &image2[0]);
			glBindTexture(GL_TEXTURE_2D, 0);

			std::wcout << "Inserting resource for Texture [" << res.path << ']' << std::endl;

			// Insert into first free cell
			for (int i = 0; i < ResourceTableSize; ++i)
				if (scResources[i].empty) {
					scResources[i].empty = FALSE;
					scResources[i].resource = res;
					return i;
				}

			std::wcout << "Can not insert Texture resource, resource table is corrupted" << std::endl;
			glDeleteTextures(1, &res.bind);

			return -1;
		}

		case AUDIO_TEXTURE: { // TODO: Load media resources
			std::wcout << "Load Resource :: Incomplete :: Audio" << std::endl;
			return -1;
		}

		case VIDEO_TEXTURE: {
			std::wcout << "Load Resource :: Incomplete :: Video" << std::endl;
			return -1;
		}

		case MIC_TEXTURE: {
			std::wcout << "Load Resource :: Incomplete :: Microphone" << std::endl;
			return -1;
		}

		case WEB_TEXTURE: {
			std::wcout << "Load Resource :: Incomplete :: Webcam" << std::endl;
			return -1;
		}

		case KEYBOARD_TEXTURE: {
			std::wcout << "Load Resource :: Incomplete :: Keyboard" << std::endl;
			return -1;
		}

		case FRAME_BUFFER: {
			res.refs = 1;

			// TODO: TRUE this frag only if the shader is used in chain of rendering main shader
			glBufferShaderShouldBeRendered[res.buffer_id] = TRUE;

			std::wcout << "Inserting resource for Buffer " << res.buffer_id << std::endl;

			// Insert into first free cell
			for (int i = 0; i < ResourceTableSize; ++i)
				if (scResources[i].empty) {
					scResources[i].empty = FALSE;
					scResources[i].resource = res;
					return i;
				}

			std::wcout << "Can not insert Buffer " << ("ABCD"[res.buffer_id]) << " resource, resource table is corrupted" << std::endl;
			return -1;
		}
	}

	return -1;
}

// Unloads the resource with given ID
// If resource is used somewhere, it's refs count decreased, else it is completely disposed.
void unloadResource(int resID) {
	
	if (scResources[resID].empty) {
		std::wcout << "Can not unload empty resource, resource table is corrupted" << std::endl;
		return;
	}

	// Check if references > 0
	if (scResources[resID].resource.refs > 1) {
		--scResources[resID].resource.refs;
		return;
	}

	if (scResources[resID].resource.refs <= 0) {
		std::wcout << "Can not unload unreferenced resource, resource table is corrupted" << std::endl;
		return;
	}
	
	// In other case unload the resource
	scResources[resID].empty = TRUE;

	// Called once when finally disposing resource:
	switch (scResources[resID].resource.type) {
		case IMAGE_TEXTURE: {
			std::wcout << "Unloading resource for Texture [" << scResources[resID].resource.path << ']' << std::endl;
			glDeleteTextures(1, &scResources[resID].resource.bind);
			return;
		}

		case AUDIO_TEXTURE: { // TODO: Unload media resources
			std::wcout << "Unload Resource :: Incomplete :: Audio" << std::endl;
			return;
		}

		case VIDEO_TEXTURE: {
			std::wcout << "Unload Resource :: Incomplete :: Video" << std::endl;
			return;
		}

		case MIC_TEXTURE: {
			std::wcout << "Unload Resource :: Incomplete :: Microphone" << std::endl;
			return;
		}

		case WEB_TEXTURE: {
			std::wcout << "Unload Resource :: Incomplete :: Webcam" << std::endl;
			return;
		}

		case FRAME_BUFFER: {
			std::wcout << "Unloading resource for Buffer " << ("ABCD"[scResources[resID].resource.buffer_id]) << std::endl;
			glBufferShaderShouldBeRendered[scResources[resID].resource.buffer_id] = FALSE;
			return;
		}
	}
}

// Performs reloading of all resources
// Returns 0 on success, 1 else
BOOL reloadResources() {
	BOOL error = FALSE;
	for (size_t i = 0; i < ResourceTableSize; ++i) {
		if (scResources[i].empty)
			continue;

		switch (scResources[i].resource.type) {
			case IMAGE_TEXTURE: {

				std::wcout << "Reloading resource for Texture [" << scResources[i].resource.path << ']' << std::endl;

				// Unload
				std::wcout << "Unloading resource for Texture [" << scResources[i].resource.path << ']' << std::endl;
				glDeleteTextures(1, &scResources[i].resource.bind);

				// Load
				std::vector<unsigned char> image;
				unsigned width, height;

				std::string stringPath = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(scResources[i].resource.path);

				unsigned error = lodepng::decode(image, width, height, stringPath);

				std::wcout << "Loading resource for Texture [" << scResources[i].resource.path << "] (" << width << ", " << height << ')' << std::endl;

				if (error != 0) {

					std::wcout << "Image resource load error: " << lodepng_error_text(error) << std::endl;
					MessageBoxA(
						NULL,
						lodepng_error_text(error),
						"Image load error",
						MB_ICONERROR | MB_OK
					);

					error = TRUE;
					break;
				}

				// Flip image vertically
				for (size_t y = 0; y < height / 2; y++)
					for (size_t x = 0; x < width; x++)
						for (size_t c = 0; c < 4; c++)
							std::swap(image[4 * width * y + 4 * x + c], image[4 * width * (height - y - 1) + 4 * x + c]);

				// Find closest power of two
				size_t u2 = 1; while (u2 < width) u2 *= 2;
				size_t v2 = 1; while (v2 < height) v2 *= 2;

				scResources[i].resource.width = u2;
				scResources[i].resource.height = v2;

				if (u2 != width)
					std::wcout << "Texture warning: width must be power of two, got " << width << ", resizing to closest " << u2 << std::endl;
				if (v2 != height)
					std::wcout << "Texture warning: height must be power of two, got " << height << ", resizing to closest " << v2 << std::endl;

				// To size of two conversion ratio
				double u3 = (double) width / u2;
				double v3 = (double) height / v2;

				// Resize texture to power of two
				std::vector<unsigned char> image2(u2 * v2 * 4);
				for (size_t y = 0; y < height; y++)
					for (size_t x = 0; x < width; x++)
						for (size_t c = 0; c < 4; c++)
							image2[4 * u2 * y + 4 * x + c] = image[4 * width * y + 4 * x + c];

				// Generate texture & pass pixeldata
				glGenTextures(1, &scResources[i].resource.bind);
				glBindTexture(GL_TEXTURE_2D, scResources[i].resource.bind);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
				glTexImage2D(GL_TEXTURE_2D, 0, 4, u2, v2, 0, GL_RGBA, GL_UNSIGNED_BYTE, &image2[0]);
				glBindTexture(GL_TEXTURE_2D, 0);

				break; // No insertion
			}

			case AUDIO_TEXTURE: { // TODO: Reload media resources
				std::wcout << "Reload Resource :: Incomplete :: Audio" << std::endl;
				break;
			}

			case VIDEO_TEXTURE: {
				std::wcout << "Reload Resource :: Incomplete :: Video" << std::endl;
				break;
			}

			case MIC_TEXTURE: {
				std::wcout << "Reload Resource :: Incomplete :: Microphone" << std::endl;
				break;
			}

			case WEB_TEXTURE: {
				std::wcout << "Reload Resource :: Incomplete :: Webcam" << std::endl;
				break;
			}

			case KEYBOARD_TEXTURE: {
				std::wcout << "Reload Resource :: Incomplete :: Keyboard" << std::endl;
				break;
			}

			case FRAME_BUFFER: {
				std::wcout << "Reloading resource for Buffer " << ("ABCD"[scResources[i].resource.buffer_id]) << " : PASS" << std::endl;
				break;
			}
		}
	}

	return error;
}

// Unloads all resources
void unloadResources() {
	for (int i = 0; i < ResourceTableSize; ++i) {
		if (scResources[i].empty)
			continue;

		unloadResource(i);

		scResources[i].empty = TRUE;
	}

	// Unload all inputs for all shaders
	for (int i = 0; i < 4; ++i) {
		scMainShaderInputs[i] = -1;

		for (int k = 0; k < 4; ++k)
			scBufferShaderInputs[i][k] = -1;
	}
}

// Performs load of main shader resource
// Warning: Path should be absolute
// Returns 0 on success, 1 else
BOOL loadMainShaderResource(SCResource res, int inputID) {

	// If main input is bound to some resource, reload this resource
	// Optionally check if target resource is loaded in this input and cancel load
	if (scMainShaderInputs[inputID] != -1) {

		int resID = findResource(res);

		// Do not relod the resource
		if (resID == scMainShaderInputs[inputID])
			return 0;

		// If resource does not exist or is not bound to this input, unload it
		unloadResource(scMainShaderInputs[inputID]);
		scMainShaderInputs[inputID] = -1;
	}

	scMainShaderInputs[inputID] = loadResource(res);
	return scMainShaderInputs[inputID] == -1;
}

// Performs unloading of main shader resource
void unloadMainShaderResource(int inputID) {

	if (scMainShaderInputs[inputID] != -1) {
		unloadResource(scMainShaderInputs[inputID]);
		scMainShaderInputs[inputID] = -1;
	}
}

// Performs load of main shader resource
// Warning: Path should be absolute
// Returns 0 on success, 1 else
BOOL loadBufferShaderResource(SCResource res, int bufferID, int inputID) {

	if (bufferID < 0 || bufferID > 3) {
		std::wcout << "Can not load resource for Buffer " << bufferID << ", Buffer does not exist, bufferID corrupt" << std::endl;
		return 1;
	}

	// If main input is bound to some resource, reload this resource
	// Optionally check if target resource is loaded in this input and cancel load
	if (scBufferShaderInputs[bufferID][inputID] != -1) {

		int resID = findResource(res);

		// Do not relod the resource
		if (resID == scBufferShaderInputs[bufferID][inputID])
			return 0;

		// If resource does not exist or is not bound to this input, unload it
		unloadResource(scBufferShaderInputs[bufferID][inputID]);
		scBufferShaderInputs[bufferID][inputID] = -1;
	}

	scBufferShaderInputs[bufferID][inputID] = loadResource(res);
	return scBufferShaderInputs[bufferID][inputID] == -1;
}

// Performs unloading of main shader resource
BOOL unloadBufferShaderResource(int bufferID, int inputID) {

	if (bufferID < 0 || bufferID > 3) {
		std::wcout << "Can not load resource for Buffer " << bufferID << ", Buffer does not exist, bufferID corrupt" << std::endl;
		return 1;
	}

	if (scBufferShaderInputs[bufferID][inputID] != -1) {
		unloadResource(scBufferShaderInputs[bufferID][inputID]);
		scBufferShaderInputs[bufferID][inputID] = -1;
	}

	return 0;
}


// Performs simple operation of opening file picker with specified extensions allowed for file open
std::wstring openFile(int fileTypesSize, const COMDLG_FILTERSPEC* fileTypes) {

	std::wstring result;
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(hr)) {

		IFileOpenDialog* pFileOpen;

		// Create the FileOpenDialog object.
		hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));

		if (SUCCEEDED(hr)) {

			// Set extension filter
			pFileOpen->SetFileTypes(fileTypesSize, fileTypes);

			// Show the Open dialog box.
			hr = pFileOpen->Show(NULL);

			// Get the file name from the dialog box.
			if (SUCCEEDED(hr)) {

				IShellItem* pItem;
				hr = pFileOpen->GetResult(&pItem);

				if (SUCCEEDED(hr)) {

					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

					// Display the file name to the user.
					if (SUCCEEDED(hr)) {
						result = pszFilePath;
						std::wcout << "File Open result :: " << result << std::endl;

						CoTaskMemFree(pszFilePath);
					}
					pItem->Release();
				}
			}
			pFileOpen->Release();
		}
		CoUninitialize();
	}

	return result;
}

// Performs simple operation of opening file picker with specified extensions allowed for file save
std::wstring saveFile(int fileTypesSize, const COMDLG_FILTERSPEC* fileTypes, const wchar_t* defaultFileName = NULL) {

	std::wstring result;
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(hr)) {

		IFileOpenDialog* pFileSave;

		// Create the FileOpenDialog object.
		hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL, IID_IFileSaveDialog, reinterpret_cast<void**>(&pFileSave));

		if (SUCCEEDED(hr)) {

			// Set extension filter
			pFileSave->SetFileTypes(fileTypesSize, fileTypes);
			if (defaultFileName != NULL)
				pFileSave->SetFileName(defaultFileName);

			// Show the Open dialog box.
			hr = pFileSave->Show(NULL);

			// Get the file name from the dialog box.
			if (SUCCEEDED(hr)) {

				IShellItem* pItem;
				hr = pFileSave->GetResult(&pItem);

				if (SUCCEEDED(hr)) {

					PWSTR pszFilePath;
					hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

					// Display the file name to the user.
					if (SUCCEEDED(hr)) {
						result = pszFilePath;
						std::wcout << "File Save result :: " << result << std::endl;

						CoTaskMemFree(pszFilePath);
					}
					pItem->Release();
				}
			}
			pFileSave->Release();
		}
		CoUninitialize();
	}

	return result;
}


struct ShaderCompilationStatus {
	GLuint shaderID = -1;
	BOOL success = FALSE;
};

// Compiles fragment shader and returns shader program ID
// Debug only
// shaderName defines the name of the shader to display if error occurs. For example BufferA or myshader.glsl
ShaderCompilationStatus compileShader(const char* fragmentSource, const char* shaderName = NULL) { // const std::wstring& fragmentSource

	// Default Vertex shader
	const char* vertexSource = R"glsl(
		#version 330 core

		in vec2 position;

		void main()
		{
			gl_Position = vec4(position, 0.0, 1.0);
		}
	)glsl";

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexSource, NULL);
	glCompileShader(vertexShader);

	GLint status;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);

	if (status != GL_TRUE) {

		// TODO: std::vector<char> or std::string buffer
		char buffer[2048];
		glGetShaderInfoLog(vertexShader, 4096, NULL, buffer);

		std::wcout << "Vertex shader compilation error: " << buffer << std::endl;
		MessageBoxA(
			NULL,
			buffer,
			"Vertex shader compilation error",
			MB_ICONERROR | MB_OK
		);

		return { 0, FALSE };
	}

	// Compile Fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
	glCompileShader(fragmentShader);

	status;
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);

	if (status != GL_TRUE) {

		// TODO: std::vector<char> or std::string buffer
		char buffer[8192];
		glGetShaderInfoLog(fragmentShader, 4096, NULL, buffer);

		if (shaderName == NULL)
			std::wcout << "Fragment shader compilation error: " << buffer << std::endl;
		else
			std::wcout << "Fragment shader " << shaderName << " compilation error: " << buffer << std::endl;
		MessageBoxA(
			NULL,
			buffer,
			shaderName == NULL ? "Fragment shader compilation error" : ("Fragment shader " + std::string(shaderName) + " compilation error").c_str(),
			MB_ICONERROR | MB_OK
		);

		return { 0, FALSE };
	}

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);


	return { shaderProgram, TRUE };
}

// Load shader and then compile
ShaderCompilationStatus compileShaderFromFile(const std::wstring& path) {
	std::ifstream f(path);
	std::string str;

	if (!f) {

		std::wcout << "Failed to load Shader file: " << "Failed to load file [" << path << ']' << std::endl;
		MessageBox(
			NULL,
			(L"Failed to load file " + path).c_str(),
			L"Failed to load Shader file",
			MB_ICONERROR | MB_OK
		);

		return { 0, FALSE };
	}

	f.seekg(0, std::ios::end);
	str.reserve(f.tellg());
	f.seekg(0, std::ios::beg);

	str.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

	return compileShader(str.c_str(), std::filesystem::path(path).filename().string().c_str());
}

// Loads Main shader from file, saves path
// Returns 0 on success, 1 else
BOOL loadMainShaderFromFile(const std::wstring& path) {
	
	// Set path for main shader in any case
	glMainShaderPath = path;

	ShaderCompilationStatus shaderResult = compileShaderFromFile(glMainShaderPath);
	if (shaderResult.success) {

		// Delete previous shader program only if load successfull
		if (glMainShaderProgramID != -1)
			glDeleteProgram(glMainShaderProgramID);

		glMainShaderProgramID = shaderResult.shaderID;

		return 0;
	}

	return 1;
}

// Reloads Main shader from saved path
// Returns 0 on success, 1 else
BOOL reloadMainShader() {

	if (glMainShaderProgramID == -1) { // Not loaded, ignore
		return 0;
	} else if (glMainShaderPath == L"") { // Loaded, but no path -> corrupt
		std::wcout << "Can not load Main shader, Shader path corrupt" << std::endl;
		return 1;
	}

	ShaderCompilationStatus shaderResult = compileShaderFromFile(glMainShaderPath);
	if (shaderResult.success) {

		// Delete previous shader program only if load successfull
		if (glMainShaderProgramID != -1)
			glDeleteProgram(glMainShaderProgramID);

		glMainShaderProgramID = shaderResult.shaderID;

		return 0;
	}

	return 1;
}

// Unloads Main shader from saved path
void unloadMainShader() {
	if (glMainShaderProgramID != -1) {
		glDeleteProgram(glMainShaderProgramID);
		glMainShaderProgramID = -1;
	}

	glMainShaderPath = L"";
}

// Loads Buffer i shader from file, saves path
// Returns 0 on success, 1 else
BOOL loadBufferShaderFromFile(const std::wstring& path, int buffer_id) {

	if (buffer_id < 0 || buffer_id > 3) {
		std::wcout << "Can not load Shader for Buffer " << buffer_id << ", Buffer does not exist, buffer_id corrupt" << std::endl;
		return 1;
	}

	// Set path for buffer shader in any case
	glBufferShaderPath[buffer_id] = path;

	ShaderCompilationStatus shaderResult = compileShaderFromFile(glBufferShaderPath[buffer_id]);
	if (shaderResult.success) {

		// Delete previous shader program only if load successfull
		if (glBufferShaderProgramIDs[buffer_id] != -1)
			glDeleteProgram(glBufferShaderProgramIDs[buffer_id]);

		glBufferShaderProgramIDs[buffer_id] = shaderResult.shaderID;

		return 0;
	}

	return 1;
}

// Loads Buffer i shader from saved path
// Returns 0 on success, 1 else
BOOL reloadBufferShader(int buffer_id) {

	if (buffer_id < 0 || buffer_id > 3) {
		std::wcout << "Can not reload Shader for Buffer " << buffer_id << ", Buffer does not exist, buffer_id corrupt" << std::endl;
		return 1;
	}

	if (glBufferShaderProgramIDs[buffer_id] == -1) { // Not loaded, ignore
		return 0;
	} else if (glBufferShaderPath[buffer_id] == L"") { // Loaded, but no path -> corrupt
		std::wcout << "Can not reload Shader for Buffer " << ("ABCD"[buffer_id]) << ", Shader path corrupt" << std::endl;
		return 1;
	}

	ShaderCompilationStatus shaderResult = compileShaderFromFile(glBufferShaderPath[buffer_id]);
	if (shaderResult.success) {

		// Delete previous shader program only if load successfull
		if (glBufferShaderProgramIDs[buffer_id] != -1)
			glDeleteProgram(glBufferShaderProgramIDs[buffer_id]);

		glBufferShaderProgramIDs[buffer_id] = shaderResult.shaderID;

		return 0;
	}

	return 1;
}

// Unloads Main shader from saved path
void unloadBufferShader(int buffer_id) {
	if (glBufferShaderProgramIDs[buffer_id] != -1) {
		glDeleteProgram(glBufferShaderProgramIDs[buffer_id]);
		glBufferShaderProgramIDs[buffer_id] = -1;
	}

	glBufferShaderPath[buffer_id] = L"";
	// scBufferFrames[bufferId] = 0;
}


// Reloads Shader Pack from scPackPath
void reloadPack() {
	if (scPackPath != L"") {
		// unload previous shaders & resources
		unloadMainShader();

		for (int i = 0; i < 4; ++i)
			if (glBufferShaderProgramIDs[i] != -1)
				unloadBufferShader(i);

		unloadResources();

		// Clear buffers
		for (int i = 0; i < 4; ++i) {

			// First
			glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[0][i]);
			glViewport(0, 0, glWidth, glHeight);
			glClearColor(0, 0, 0, 0);
			glClear(GL_COLOR_BUFFER_BIT);

			// Second
			glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[1][i]);
			glViewport(0, 0, glWidth, glHeight);
			glClearColor(0, 0, 0, 0);
			glClear(GL_COLOR_BUFFER_BIT);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			scBufferFrames[i] = 0;
		}

		glViewport(0, 0, glWidth, glHeight);
		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);

		// Reset time & frame
		glfwSetTime(0.0);
		scTimestamp = 0.0;
		scFrames = 0;

		// Read JSON from path & validate
		std::ifstream f(scPackPath);
		std::string str;

		if (!f) {

			std::wcout << "JSON :: Failed to open Pack file :: " << scPackPath << std::endl;
			MessageBox(
				NULL,
				(L"Failed to open file " + scPackPath).c_str(),
				L"Failed to load Pack file",
				MB_ICONERROR | MB_OK
			);

			return;
		}

		f.seekg(0, std::ios::end);
		str.reserve(f.tellg());
		f.seekg(0, std::ios::beg);

		str.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

		try {

			auto j = nlohmann::json::parse(str);

			if (!j.contains("Main")) {

				std::wcout << "JSON :: Missing Main section in Pack file :: " << scPackPath << std::endl;
				MessageBox(
					NULL,
					(L"Missing Main section in Pack file\n" + scPackPath).c_str(),
					L"Failed to setup Pack",
					MB_ICONERROR | MB_OK
				);

				return;
			}

			auto mainShader = j["Main"];

			// Section "Main" in JSON is one of the following types:
			// 1. json:string: only path to main shader, does not contain buffers or inputs
			//    Most common: { "Main": "/home/work/bed/forever.glsl" }
			//
			// 2. json:object containing:
			//    1. "path": json:string path to main shader file (relative to program or absolute) (mandatory)
			//    2. "inputs": json:array of json:objects (optional) describing shader inputs containing:
			//       1. "type": json:string one of following (any case) (mandatory):
			//          1. BufferA
			// 		    2. BufferB
			// 		    3. BufferC
			// 		    4. BufferD
			//          5. Image
			// 		    6. Microphone
			// 		    7. Webcamera
			// 		    8. Keyboard
			// 		    9. Audio
			// 		    10. Video
			// 	     2. "path": json:string path to file location (relative to program or absolute) (only for Image, Video and Audio types)

			if (mainShader.is_string()) {

				// Try to open shader from file and don't care
				// Preserve old path
				auto old_path = std::filesystem::current_path();
				auto parent_path = std::filesystem::path(scPackPath).parent_path();
				std::filesystem::current_path(parent_path);

				// Construct absolute path
				std::wstring path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(mainShader);
				path = std::filesystem::weakly_canonical(parent_path / std::filesystem::path(path));

				// Restore old path
				std::filesystem::current_path(old_path);

				std::wcout << "JSON :: Main Shader path :: " << path << std::endl;

				loadMainShaderFromFile(path);

				return;

			} else if (mainShader.is_object()) {

				// Validate containing Main shader path
				if (!mainShader.contains("path") || !mainShader["path"].is_string()) {

					std::wcout << "JSON :: Main section should contain path string :: " << scPackPath << std::endl;
					MessageBox(
						NULL,
						(L"Main section should contain path string\n" + scPackPath).c_str(),
						L"Failed to setup Pack",
						MB_ICONERROR | MB_OK
					);

					return;
				}

				// Try to open shader from file and don't care
				// Preserve old path
				auto old_path = std::filesystem::current_path();
				auto parent_path = std::filesystem::path(scPackPath).parent_path();
				std::filesystem::current_path(parent_path);

				// Construct absolute path
				std::wstring path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(mainShader["path"]);
				path = std::filesystem::weakly_canonical(parent_path / std::filesystem::path(path));

				// Restore old path
				std::filesystem::current_path(old_path);

				std::wcout << "JSON :: Main Shader path :: " << path << std::endl;

				loadMainShaderFromFile(path);

				if (mainShader.contains("inputs")) {
					if (!mainShader["inputs"].is_array()) {

						std::wcout << "JSON :: Inputs entry of Main section should be array :: " << scPackPath << std::endl;
						MessageBox(
							NULL,
							(L"Inputs entry of Main section should be array\n" + scPackPath).c_str(),
							L"Failed to setup Pack",
							MB_ICONERROR | MB_OK
						);

						return;
					}

					// Not error, however..
					if (mainShader["inputs"].size() < 4 || mainShader["inputs"].size() > 4)
						std::wcout << "Warning :: JSON :: Main Shader has " << mainShader["inputs"].size() << " inputs, but 4 required :: " << scPackPath << std::endl;

					for (int i = 0; i < mainShader["inputs"].size(); ++i) {

						if (i >= 4)
							break;

						auto input = mainShader["inputs"][i];

						if (input.is_null()) // Skip null
							continue;

						if (!input.is_object()) { // Input must be object or string buffer name

							if (!input.is_string()) {
								std::wcout << "JSON :: Input " << i << " of section Main should be object or string containing input Buffer name :: " << scPackPath << std::endl;
								MessageBox(
									NULL,
									(L"Input " + std::to_wstring(i) + L" of section Main should be object or string containing input Buffer name\n" + scPackPath).c_str(),
									L"Failed to setup Pack",
									MB_ICONERROR | MB_OK
								);

								return;
							}

							std::wstring type = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(input.get<std::string>());

							transform(type.begin(), type.end(), type.begin(), ::towlower);

							SCResource res;
							res.type = FRAME_BUFFER;

							if (type == L"buffera") // Buffer A input
								res.buffer_id = 0;
							else if (type == L"bufferb") // Buffer B input
								res.buffer_id = 1;
							else if (type == L"bufferc") // Buffer C input
								res.buffer_id = 2;
							else if (type == L"bufferd")  // Buffer D input
								res.buffer_id = 3;
							else if (type == L"")
								continue;
							else {
								std::wcout << "JSON :: Input " << i << " of section Main contains invalid buffer name " << type << " :: " << scPackPath << std::endl;
								MessageBox(
									NULL,
									(L"Input " + std::to_wstring(i) + L" of section Main contains invalid buffer name " + type + L"\n" + scPackPath).c_str(),
									L"Failed to setup Pack",
									MB_ICONERROR | MB_OK
								);

								return;
							}

							loadMainShaderResource(res, i);

							continue;
						}

						if (!input.contains("type") || !input["type"].is_string()) {

							std::wcout << "JSON :: Input " << i << " of section Main should have string containing type :: " << scPackPath << std::endl;
							MessageBox(
								NULL,
								(L"Input " + std::to_wstring(i) + L" of section Main should have string containing type\n" + scPackPath).c_str(),
								L"Failed to setup Pack",
								MB_ICONERROR | MB_OK
							);

							return;
						}

						std::wstring type = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(input["type"].get<std::string>());

						transform(type.begin(), type.end(), type.begin(), ::towlower);

						SCResource res;
						bool path_required = false;
						bool assume_empty = false;

						// 1. BufferA
						// 2. BufferB
						// 3. BufferC
						// 4. BufferD
						// 5. Image
						// 6. Microphone
						// 7. Webcamera
						// 8. Keyboard
						// 9. Audio
						// 10. Video

						if (type == L"buffera") { // Buffer A input
							res.type = FRAME_BUFFER;
							res.buffer_id = 0;
						} else if (type == L"bufferb") { // Buffer B input
							res.type = FRAME_BUFFER;
							res.buffer_id = 1;
						} else if (type == L"bufferc") { // Buffer C input
							res.type = FRAME_BUFFER;
							res.buffer_id = 2;
						} else if (type == L"bufferd") { // Buffer D input
							res.type = FRAME_BUFFER;
							res.buffer_id = 3;
						} else if (type == L"image") { // Image input
							res.type = IMAGE_TEXTURE;
							path_required = true;
						} else if (type == L"microphone") { // Microphone
							std::wcout << "Incomplete :: JSON :: Main Shader Input :: Microphone" << std::endl;
							assume_empty = true;
						} else if (type == L"webcamera") { // Webcamera
							std::wcout << "Incomplete :: JSON :: Main Shader Input :: Webcamera" << std::endl;
							assume_empty = true;
						} else if (type == L"keyboard") { // Keyboard
							std::wcout << "Incomplete :: JSON :: Main Shader Input :: Keyboard" << std::endl;
							assume_empty = true;
						} else if (type == L"audio") { // Audio
							std::wcout << "Incomplete :: JSON :: Main Shader Input :: Audio" << std::endl;
							assume_empty = true;
						} else if (type == L"video") { // Video
							std::wcout << "Incomplete :: JSON :: Main Shader Input :: Video" << std::endl;
							assume_empty = true;
						} else {
							std::wcout << "JSON :: Input " << i << " of section Main is unsupported type " << type << " :: " << scPackPath << std::endl;
							MessageBox(
								NULL,
								(L"Input " + std::to_wstring(i) + L" of section Main is unsupported type " + type + L"\n" + scPackPath).c_str(),
								L"Failed to setup Pack",
								MB_ICONERROR | MB_OK
							);

							return;
						}

						if (assume_empty)
							continue;

						if (path_required) {
							if (!input.contains("path") || !input["path"].is_string()) {

								std::wcout << "JSON :: Input " << i << " of section Main should have string containing path :: " << scPackPath << std::endl;
								MessageBox(
									NULL,
									(L"Input " + std::to_wstring(i) + L" of section Main should have string containing path\n" + scPackPath).c_str(),
									L"Failed to setup Pack",
									MB_ICONERROR | MB_OK
								);

								return;
							}

							// Try to open shader from file and don't care
							// Preserve old path
							auto old_path = std::filesystem::current_path();
							auto parent_path = std::filesystem::path(scPackPath).parent_path();
							std::filesystem::current_path(parent_path);

							// Construct absolute path
							std::wstring path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(input["path"]);
							path = std::filesystem::weakly_canonical(parent_path / std::filesystem::path(path));

							// Restore old path
							std::filesystem::current_path(old_path);

							std::wcout << "JSON :: Main Shader Input " << i << " path :: " << path << std::endl;

							res.path = path;
						}

						loadMainShaderResource(res, i);
					}
				}

			} else {
				std::wcout << "JSON :: Section Main should be object or string path to file :: " << scPackPath << std::endl;
				MessageBox(
					NULL,
					(L"Section Main should be object or string path to file\n" + scPackPath).c_str(),
					L"Failed to setup Pack",
					MB_ICONERROR | MB_OK
				);

				return;
			}

			// Do the same for each buffer
			for (int k = 0; k < 4; ++k) {
				std::string bufferKey = std::string("Buffer") + "ABCD"[k];
				std::wstring bufferKeyW = std::wstring(L"Buffer") + L"ABCD"[k];

				if (!j.contains(bufferKey))
					continue;

				auto bufferShader = j[bufferKey];

				if (bufferShader.is_null())
					continue;

				// Section "Buffer[A/B/C/D]" in JSON is one of the following types:
				// 1. json:string: only path to Buffer shader, does not have inputs
				//    Most common: { "BufferA": "/i/love/hedgehogs.glsl" }
				//
				// 2. json:object containing:
				//    1. "path": json:string path to Buffer shader file (relative to program or absolute) (mandatory)
				//    2. "inputs": json:array of json:objects (optional) describing shader inputs containing:
				//       1. "type": json:string one of following (any case) (mandatory):
				//          1. BufferA
				// 		    2. BufferB
				// 		    3. BufferC
				// 		    4. BufferD
				//          5. Image
				// 		    6. Microphone
				// 		    7. Webcamera
				// 		    8. Keyboard
				// 		    9. Audio
				// 		    10. Video
				// 	     2. "path": json:string path to file location (relative to program or absolute) (only for Image, Video and Audio types)

				if (bufferShader.is_string()) {

					// Try to open shader from file and don't care
					// Preserve old path
					auto old_path = std::filesystem::current_path();
					auto parent_path = std::filesystem::path(scPackPath).parent_path();
					std::filesystem::current_path(parent_path);

					// Construct absolute path
					std::wstring path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(bufferShader);
					path = std::filesystem::weakly_canonical(parent_path / std::filesystem::path(path));

					// Restore old path
					std::filesystem::current_path(old_path);

					std::wcout << "JSON :: " << bufferKeyW << " path :: " << path << std::endl;

					loadMainShaderFromFile(path);

					continue;

				} else if (bufferShader.is_object()) {

					// Validate containing Main shader path
					if (!bufferShader.contains("path") || !bufferShader["path"].is_string()) {

						std::wcout << "JSON :: " << bufferKeyW << " section should contain path string :: " << scPackPath << std::endl;
						MessageBox(
							NULL,
							(bufferKeyW + L" section should contain path string\n" + scPackPath).c_str(),
							L"Failed to setup Pack",
							MB_ICONERROR | MB_OK
						);

						return;
					}

					// Try to open shader from file and don't care
					// Preserve old path
					auto old_path = std::filesystem::current_path();
					auto parent_path = std::filesystem::path(scPackPath).parent_path();
					std::filesystem::current_path(parent_path);

					// Construct absolute path
					std::wstring path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(bufferShader["path"]);
					path = std::filesystem::weakly_canonical(parent_path / std::filesystem::path(path));

					// Restore old path
					std::filesystem::current_path(old_path);

					std::wcout << "JSON :: " << bufferKeyW << " Shader path :: " << path << std::endl;

					loadBufferShaderFromFile(path, k);

					if (bufferShader.contains("inputs")) {
						if (!bufferShader["inputs"].is_array()) {

							std::wcout << "JSON :: Inputs entry of " << bufferKeyW << " section should be array :: " << scPackPath << std::endl;
							MessageBox(
								NULL,
								(L"Inputs entry of " + bufferKeyW + L" section should be array\n" + scPackPath).c_str(),
								L"Failed to setup Pack",
								MB_ICONERROR | MB_OK
							);

							return;
						}

						if (bufferShader["inputs"].size() < 4 || bufferShader["inputs"].size() > 4)
							std::wcout << "Warning :: JSON :: " << bufferKeyW << " Shader has " << bufferShader["inputs"].size() << " inputs, but 4 required :: " << scPackPath << std::endl;

						for (int i = 0; i < bufferShader["inputs"].size(); ++i) {

							if (i >= 4)
								break;

							auto input = bufferShader["inputs"][i];

							if (input.is_null()) // Skip null
								continue;

							if (!input.is_object()) { // Input must be object or string buffer name

								if (!input.is_string()) {
									std::wcout << "JSON :: Input " << std::to_wstring(i) << " of section " << bufferKeyW << " should be object or string containing input Buffer name" << scPackPath << std::endl;
									MessageBox(
										NULL,
										(bufferKeyW + L" Shader Input " + std::to_wstring(i) + L" should be object or string containing input Buffer name\n" + scPackPath).c_str(),
										L"Failed to setup Pack",
										MB_ICONERROR | MB_OK
									);

									return;
								}

								std::wstring type = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(input.get<std::string>());

								transform(type.begin(), type.end(), type.begin(), ::towlower);

								SCResource res;
								res.type = FRAME_BUFFER;

								if (type == L"buffera") // Buffer A input
									res.buffer_id = 0;
								else if (type == L"bufferb") // Buffer B input
									res.buffer_id = 1;
								else if (type == L"bufferc") // Buffer C input
									res.buffer_id = 2;
								else if (type == L"bufferd")  // Buffer D input
									res.buffer_id = 3;
								else if (type == L"")
									continue;
								else {
									std::wcout << "JSON :: Input " << i << " of section " << bufferKeyW << " contains invalid buffer name " << type << " :: " << scPackPath << std::endl;
									MessageBox(
										NULL,
										(L"Input " + std::to_wstring(i) + L" of section " + bufferKeyW + L" contains invalid buffer name " + type + L"\n" + scPackPath).c_str(),
										L"Failed to setup Pack",
										MB_ICONERROR | MB_OK
									);

									return;
								}

								loadBufferShaderResource(res, k, i);

								continue;
							}

							if (!input.contains("type") || !input["type"].is_string()) {

								std::wcout << "JSON :: Input " << i << " of section " << bufferKeyW << " should have string containing type :: " << scPackPath << std::endl;
								MessageBox(
									NULL,
									(L"Input " + std::to_wstring(i) + L" of section " + bufferKeyW + L" should have string containing type\n" + scPackPath).c_str(),
									L"Failed to setup Pack",
									MB_ICONERROR | MB_OK
								);

								return;
							}

							std::wstring type = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(input["type"].get<std::string>());

							transform(type.begin(), type.end(), type.begin(), ::towlower);

							SCResource res;
							bool path_required = false;
							bool assume_empty = false;

							// 1. BufferA
							// 2. BufferB
							// 3. BufferC
							// 4. BufferD
							// 5. Image
							// 6. Microphone
							// 7. Webcamera
							// 8. Keyboard
							// 9. Audio
							// 10. Video

							if (type == L"buffera") { // Buffer A input
								res.type = FRAME_BUFFER;
								res.buffer_id = 0;
							} else if (type == L"bufferb") { // Buffer B input
								res.type = FRAME_BUFFER;
								res.buffer_id = 1;
							} else if (type == L"bufferc") { // Buffer C input
								res.type = FRAME_BUFFER;
								res.buffer_id = 2;
							} else if (type == L"bufferd") { // Buffer D input
								res.type = FRAME_BUFFER;
								res.buffer_id = 3;
							} else if (type == L"image") { // Image input
								res.type = IMAGE_TEXTURE;
								path_required = true;
							} else if (type == L"microphone") { // Microphone
								std::wcout << "Incomplete :: JSON :: " << bufferKeyW << " Shader Input :: Microphone" << std::endl;
								assume_empty = true;
							} else if (type == L"webcamera") { // Webcamera
								std::wcout << "Incomplete :: JSON :: " << bufferKeyW << " Shader Input :: Webcamera" << std::endl;
								assume_empty = true;
							} else if (type == L"keyboard") { // Keyboard
								std::wcout << "Incomplete :: JSON :: " << bufferKeyW << " Shader Input :: Keyboard" << std::endl;
								assume_empty = true;
							} else if (type == L"audio") { // Audio
								std::wcout << "Incomplete :: JSON :: " << bufferKeyW << " Shader Input :: Audio" << std::endl;
								assume_empty = true;
							} else if (type == L"video") { // Video
								std::wcout << "Incomplete :: JSON :: " << bufferKeyW << " Shader Input :: Video" << std::endl;
								assume_empty = true;
							} else {
								std::wcout << "JSON :: Input " << i << " of section " << bufferKeyW << " is unsupported type " << type << " :: " << scPackPath << std::endl;
								MessageBox(
									NULL,
									(L"Input " + std::to_wstring(i) + L" of section " + bufferKeyW + L" is unsupported type " + type + L"\n" + scPackPath).c_str(),
									L"Failed to setup Pack",
									MB_ICONERROR | MB_OK
								);

								return;
							}

							if (assume_empty)
								continue;

							if (path_required) {
								if (!input.contains("path") || !input["path"].is_string()) {

									std::wcout << "JSON :: Input " << i << " of section " << bufferKeyW << " should have string containing path :: " << scPackPath << std::endl;
									MessageBox(
										NULL,
										(L"Input " + std::to_wstring(i) + L" of section " + bufferKeyW + L" should have string containing path\n" + scPackPath).c_str(),
										L"Failed to setup Pack",
										MB_ICONERROR | MB_OK
									);

									return;
								}

								// Try to open shader from file and don't care
								// Preserve old path
								auto old_path = std::filesystem::current_path();
								auto parent_path = std::filesystem::path(scPackPath).parent_path();
								std::filesystem::current_path(parent_path);

								// Construct absolute path
								std::wstring path = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(input["path"]);
								path = std::filesystem::weakly_canonical(parent_path / std::filesystem::path(path));

								// Restore old path
								std::filesystem::current_path(old_path);

								std::wcout << "JSON :: " << bufferKeyW << " Shader Input " << i << " path :: " << path << std::endl;

								res.path = path;
							}

							loadBufferShaderResource(res, k, i);
						}
					}

				} else {
					std::wcout << "Failed to parse Pack file :: Main Shader should be string path to file or object :: " << scPackPath << std::endl;
					MessageBox(
						NULL,
						(L"Main Shader should be string path to file or object\n" + scPackPath).c_str(),
						L"Failed to setup Pack",
						MB_ICONERROR | MB_OK
					);

					return;
				}
			}

		} catch (const nlohmann::json::exception& ex) { // Catch JSON exceptions

			std::wcout << "JSON :: Failed to parse Pack JSON :: " << ex.what() << " :: " << scPackPath << std::endl;
			std::string what = ex.what();
			std::wstring wwhatt/*!!??*/ = std::wstring(what.begin(), what.end());
			MessageBox(
				NULL,
				(L"Failed to parse Pack JSON:\n" + wwhatt + L"\n" + scPackPath).c_str(),
				L"Failed to parse Pack",
				MB_ICONERROR | MB_OK
			);
		} catch (...) { // Catch other exceptions

			std::wcout << "Failed to parse Pack with Unresolved exception :: " << scPackPath << std::endl;
			MessageBox(
				NULL,
				(L"Failed to parse Pack with Unresolved exception\n" + scPackPath).c_str(),
				L"Failed to parse Pack",
				MB_ICONERROR | MB_OK
			);
		}
	}
}

// Saves pack to scPackPath
void savePack() {
	if (scPackPath == L"")
		return;

	/*
	if (glMainShaderPath == L"") {
		std::wcout << "To JSON :: Pack should have a Main Shader :: " << scPackPath << std::endl;

		MessageBox(
			NULL,
			(L"Pack should have a Main Shader\n" + scPackPath).c_str(),
			L"Failed to save Pack",
			MB_ICONERROR | MB_OK
		);

		return;
	}
	*/

	std::filesystem::path basePackPath = std::filesystem::path(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(scPackPath)).parent_path();
	
	nlohmann::json j;

	// Main shader is not required, however..
	if (glMainShaderPath != L"") {
		j["Main"]["path"] = std::filesystem::relative(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(glMainShaderPath), basePackPath).string();

		j["Main"]["inputs"] = nlohmann::json::array();

		for (int i = 0; i < 4; ++i) {
			if (scMainShaderInputs[i] == -1)
				continue;
			//j["Main"]["inputs"][i] = nullptr;
			else {
				if (scResources[scMainShaderInputs[i]].empty)
					std::wcout << "To JSON :: Main Shader input " << i << " is empty, scResources corrupt" << std::endl;
				else {
					switch (scResources[scMainShaderInputs[i]].resource.type) {
						case IMAGE_TEXTURE: {
							j["Main"]["inputs"][i]["type"] = "Image";
							j["Main"]["inputs"][i]["path"] = std::filesystem::relative(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(scResources[scMainShaderInputs[i]].resource.path), basePackPath).string();
							break;
						}

						case AUDIO_TEXTURE: { // TODO: Save media resources
							std::wcout << "To JSON :: Main Shader :: Incomplete :: Audio" << std::endl;
							break;
						}

						case VIDEO_TEXTURE: {
							std::wcout << "To JSON :: Main Shader :: Incomplete :: Video" << std::endl;
							break;
						}

						case MIC_TEXTURE: {
							std::wcout << "To JSON :: Main Shader :: Incomplete :: Microphone" << std::endl;
							break;
						}

						case WEB_TEXTURE: {
							std::wcout << "To JSON :: Main Shader :: Incomplete :: Webcam" << std::endl;
							break;
						}

						case KEYBOARD_TEXTURE: {
							std::wcout << "To JSON :: Main Shader :: Incomplete :: Keyboard" << std::endl;
							break;
						}

						case FRAME_BUFFER: {
							j["Main"]["inputs"][i]["type"] = std::string("Buffer") + "ABCD"[scResources[scMainShaderInputs[i]].resource.buffer_id];
							break;
						}
					}
				}
			}
		}
	}

	// Do the same for buffers
	for (int k = 0; k < 4; ++k) {

		if (glBufferShaderPath[k] == L"")
			continue;

		std::string bufferKey = std::string("Buffer") + "ABCD"[k];

		j[bufferKey]["path"] = std::filesystem::relative(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(glBufferShaderPath[k]), basePackPath).string();
		j[bufferKey]["inputs"] = nlohmann::json::array();

		for (int i = 0; i < 4; ++i) {
			if (scBufferShaderInputs[k][i] == -1)
				continue;
				//j[bufferKey]["inputs"][i] = nullptr;
			else {
				if (scResources[scBufferShaderInputs[k][i]].empty)
					std::wcout << "To JSON :: Buffer " << ("ABCD"[k]) << " Shader input " << i << " is empty, scResources corrupt" << std::endl;
				else {
					switch (scResources[scBufferShaderInputs[k][i]].resource.type) {
						case IMAGE_TEXTURE: {
							j[bufferKey]["inputs"][i]["type"] = "Image";
							j[bufferKey]["inputs"][i]["path"] = std::filesystem::relative(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(scResources[scBufferShaderInputs[k][i]].resource.path), basePackPath).string();
							break;
						}

						case AUDIO_TEXTURE: { // TODO: Save media resources
							std::wcout << "To JSON :: Buffer " << ("ABCD"[k]) << " Shader :: Incomplete :: Audio" << std::endl;
							break;
						}

						case VIDEO_TEXTURE: {
							std::wcout << "To JSON :: Buffer " << ("ABCD"[k]) << " Shader :: Incomplete :: Video" << std::endl;
							break;
						}

						case MIC_TEXTURE: {
							std::wcout << "To JSON :: Buffer " << ("ABCD"[k]) << " Shader :: Incomplete :: Microphone" << std::endl;
							break;
						}

						case WEB_TEXTURE: {
							std::wcout << "To JSON :: Buffer " << ("ABCD"[k]) << " Shader :: Incomplete :: Webcam" << std::endl;
							break;
						}

						case KEYBOARD_TEXTURE: {
							std::wcout << "To JSON :: Buffer " << ("ABCD"[i]) << " Shader :: Incomplete :: Keyboard" << std::endl;
							break;
						}

						case FRAME_BUFFER: {
							j[bufferKey]["inputs"][i]["type"] = std::string("Buffer") + "ABCD"[scResources[scBufferShaderInputs[k][i]].resource.buffer_id];
							break;
						}
					}
				}
			}
		}
	}

	// Save pack file
	std::ofstream out(scPackPath.c_str());
	if (!out) {
		std::wcout << "To JSON :: Failed to create Pack file :: " << scPackPath << std::endl;

		MessageBox(
			NULL,
			(L"Failed to create Pack file " + scPackPath).c_str(),
			L"Failed to create file",
			MB_ICONERROR | MB_OK
		);

		return;
	}
	out << j.dump(4);
	out.close();
}


// Used to rapaint desktop window to prevent artifacts
void repaintDesktop() {
	// Locate WorkerW
	HWND workerw = WorkerWEnumerator::enumerateForWorkerW();

	if (workerw == NULL) {
		std::wcout << "WorkerW enumeration failed" << std::endl;
		return;
	}

	RedrawWindow(workerw, NULL, NULL, RDW_INTERNALPAINT | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_INVALIDATE);

	// TODO: Figure out how to repaint desktop window, lol
	
	//int HWND_BROADCAST = 0xffff;
	//int WM_SETTINGCHANGE = 0x1a;
	//int SMTO_ABORTIFHUNG = 0x0002;

	//SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, NULL, NULL, SMTO_ABORTIFHUNG, 100, NULL);
	
	//UpdatePerUserSystemParameter(1, TRUE);
	
	//FillRect();
	//RECT rect;
	//::GetClientRect(::GetDesktopWindow(), &rect);
	//::RedrawWindow(::GetDesktopWindow(), &rect, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Initialize the OpenGL scene
void initSC() {

	// Here be dragons
	glViewport(0, 0, glWidth, glHeight);
	glClearColor(0, 0, 0, 0);

	glfwSetTime(0.0);

	// Cool GL stuff (c)
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);

	// Reference for stupid me: https://open.gl/drawing

	// Generate Array buffer with vertices of square
	const float squareVertices[] = {
		 1.0f,  1.0f,
		 1.0f, -1.0f,
		-1.0f, -1.0f,
		-1.0f,  1.0f
	};

	const GLuint squareIndices[] = {
		0, 1, 2,
		2, 3, 0
	};

	glGenVertexArrays(1, &glSquareVAO);
	glGenBuffers(1, &glSquareVBO);
	glGenBuffers(1, &glSquareEBO);

	glBindVertexArray(glSquareVAO);

	glBindBuffer(GL_ARRAY_BUFFER, glSquareVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(squareVertices), squareVertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glSquareEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(squareIndices), squareIndices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (GLvoid*) 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);


	// Create buffer and texture for all buffers (4 in total)
	glGenFramebuffers(4, glBufferShaderFramebuffers[0]);
	glGenFramebuffers(4, glBufferShaderFramebuffers[1]);
	glGenTextures(4, glBufferShaderFramebufferTextures[0]);
	glGenTextures(4, glBufferShaderFramebufferTextures[1]);

	for (int i = 0; i < 4; ++i) {
		// First
		glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[0][i]);
		glViewport(0, 0, glWidth, glHeight);

		glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[0][i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glWidth, glHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, glBufferShaderFramebufferTextures[0][i], 0);

		// Second
		glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[1][i]);
		glViewport(0, 0, glWidth, glHeight);

		glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[1][i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glWidth, glHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, glBufferShaderFramebufferTextures[1][i], 0);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

// Resize the OpenGL Scene
// values automatically calculated from [currentWindowDimensions]
void resizeSC() {
	glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
	glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

	glViewport(0, 0, glWidth, glHeight);

	// Resize all buffer textures
	for (int i = 0; i < 4; ++i) {
		// First
		glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[0][i]);
		glViewport(0, 0, glWidth, glHeight);

		glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[0][i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glWidth, glHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);

		// Second
		glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[1][i]);
		glViewport(0, 0, glWidth, glHeight);

		glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[1][i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glWidth, glHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

// Render single frame of the Scene
void renderSC() {

	if (glMainShaderProgramID != -1) {

		if (!scPaused) {

			// Each shader has following inputs (From shadertoy.com):
			// 
			// Basic:
			// uniform vec3      iResolution;           // viewport resolution (in pixels)
			// uniform float     iTime;                 // shader playback time (in seconds)
			// uniform float     iTimeDelta;            // render time (in seconds)
			// uniform int       iFrame;                // shader playback frame
			// uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
			// uniform vec4      iDate;                 // (year, month, day, time in seconds)
			// 
			// Advanced:
			// uniform float     iChannelTime[4];       // channel playback time (in seconds)
			// uniform vec3      iChannelResolution[4]; // channel resolution (in pixels)
			// uniform samplerXX iChannel0..3;          // input channel. XX = 2D/Cube
			// 
			// uniform float     iSampleRate;           // sound sample rate (i.e., 44100)

			// Basic values:
			time_t t = std::time(NULL);
			tm* timePtr = std::localtime(&t);

			// Warning: If you are from 16,777,215 earth year, this programm will stop normally function in the next year
			//          If you are not from earth or do not rely on gregorian calendar, ignore this message
			//          Si per accidens, vocavit ab exterioris ratio in usu hoc progressio, vos can contact bitrate16@gmail.com 
			//           ad tollendam hac, et revertere ad pristina circulus inferni.
			//          In case of containment breach caused by this program, activate protocol SCP-[DELETED]-Alpha to avoid [DATA CORRUPTED].
			//          R WAAQ COS DaR AMITo 7 > DOS mot
			// TODO: tuturudu
			float iDate_year = (float) timePtr->tm_year;
			float iDate_month = (float) timePtr->tm_mon;
			float iDate_day = (float) timePtr->tm_mday;
			float iDate_time = (float) (timePtr->tm_hour * 60 * 60 + timePtr->tm_min * 60 + timePtr->tm_sec);

			// Calculate mouse location (if enabled)
			POINT currentMouse = { 0, 0 };

			if (scMouseEnabled) {
				if (!GetCursorPos(&currentMouse))
					currentMouse = { 0, 0 };
				else {
					// Compute mouse relative to each display in case of single screen
					currentMouse.x -= currentWindowDimensions.left;
					currentMouse.y -= currentWindowDimensions.top;
					currentMouse.y = currentWindowDimensions.bottom + currentWindowDimensions.top - currentMouse.y;
				}
			}

			// TODO: Move following code out of renderSC to prevent resource evaluation count in scene render time
			// TODO: Evaluate dynamic resources (Video / webcam frames, audio / microphone FFT's)
			for (size_t i = 0; i < ResourceTableSize; ++i) {
				if (scResources[i].empty)
					continue;

				switch (scResources[i].resource.type) {
					case AUDIO_TEXTURE: { // TODO: Evaluate dynamic resources media resources
						break;
					}

					case VIDEO_TEXTURE: {
						break;
					}

					case MIC_TEXTURE: {
						break;
					}

					case WEB_TEXTURE: {
						break;
					}

					case KEYBOARD_TEXTURE: {
						break;
					}
				}
			}

			// Evaluate buffers
			// Constant names for optimize speed:
			const char* const iChannelResolutionUniforms[4] = {
				"iChannelResolution[0]",
				"iChannelResolution[1]",
				"iChannelResolution[2]",
				"iChannelResolution[3]"
			};

			const char* const iChannelTimeUniforms[4] = {
				"iChannelTime[0]",
				"iChannelTime[1]",
				"iChannelTime[2]",
				"iChannelTime[3]"
			};

			const char* const iChannelUniforms[4] = {
				"iChannel0",
				"iChannel1",
				"iChannel2",
				"iChannel3"
			};

			// Render all buffers
			// TODO: Asynchronous buffer & main shader rendering
			for (int i = 0; i < 4; ++i) {

				// Require both conditions to complete in order to render
				if (glBufferShaderShouldBeRendered[i] && glBufferShaderProgramIDs[i] != -1) {

					glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[(scBufferFrames[i] + 1) & 1][i]);
					glViewport(0, 0, glWidth, glHeight);
					glClearColor(0, 0, 0, 0);
					glClear(GL_COLOR_BUFFER_BIT);

					glUseProgram(glBufferShaderProgramIDs[i]);

					// Load all Basic inputs
					glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iResolution"), (float) glWidth, (float) glHeight, 0.0);
					glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iTime"), (float) glfwGetTime());
					glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iTimeDelta"), (float) (glfwGetTime() - scTimestamp));
					glUniform1i(glGetUniformLocation(glBufferShaderProgramIDs[i], "iFrame"), scFrames); // TODO: Should we pass actual buffer frames or global scFrames is enough?

					// TODO: Validate iMouse.zw data
					if (scMouseEnabled) 
						glUniform4f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iMouse"), (GLfloat) currentMouse.x, (GLfloat) currentMouse.y, (GLfloat) scMouse.x, (GLfloat) scMouse.y);
					else
						glUniform4f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iMouse"), (GLfloat) scMouse.x, (GLfloat) scMouse.y, 0, 0);

					glUniform4f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iDate"), (GLfloat) iDate_year, (GLfloat) iDate_month, (GLfloat) iDate_day, (GLfloat) iDate_time);

					// Default value for SampleRate
					// TODO: Should evaluate from inputs
					glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], "iSampleRate"), 0);

					// Bind iChannel data
					for (int k = 0; k < 4; ++k) {
						if (scBufferShaderInputs[i][k] == -1) {
							glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
							glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
							glUniform1i(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelUniforms[k]), 0); // GL_TEXTURE0 which is unused
							continue;
						}

						if (scResources[scBufferShaderInputs[i][k]].empty) {
							std::wcout << "Can not configure iChannelResolution for input " << k << " in Buffer " << i << ", input points to empty resource, scResources corrupt" << std::endl;
							glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
							glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
							glUniform1i(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelUniforms[k]), 0); // GL_TEXTURE0 which is unused
							continue;
						}

						switch (scResources[scBufferShaderInputs[i][k]].resource.type) {
							case IMAGE_TEXTURE: {

								// Bind texture
								// TODO: Support for Sampler3D (requires shader recompile), e.t.c.
								glActiveTexture(GL_TEXTURE5 + i * 4 + k);
								glBindTexture(GL_TEXTURE_2D, scResources[scBufferShaderInputs[i][k]].resource.bind);
								glUniform1i(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelUniforms[k]), 5 + i * 4 + k);

								// Width & Height 
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) scResources[scBufferShaderInputs[i][k]].resource.width, (GLfloat) scResources[scBufferShaderInputs[i][k]].resource.height, (GLfloat) 0);

								// Timestamp 0
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);

								continue;
							}

							case AUDIO_TEXTURE: { // TODO: Compute input dimensions
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case VIDEO_TEXTURE: {
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case MIC_TEXTURE: {
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case WEB_TEXTURE: {
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case KEYBOARD_TEXTURE: {
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) 0);
								continue;
							}

							case FRAME_BUFFER: { // Buffer size always match the viewport size
								
								// Bind texture
								glActiveTexture(GL_TEXTURE5 + i * 4 + k);
								glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[scBufferFrames[scResources[scBufferShaderInputs[i][k]].resource.buffer_id] & 1][scResources[scBufferShaderInputs[i][k]].resource.buffer_id]);
								glUniform1i(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelUniforms[k]), 5 + i * 4 + k);

								// Width & Height 
								glUniform3f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelResolutionUniforms[k]), (GLfloat) glWidth, (GLfloat) glHeight, (GLfloat) 0);
								
								// Timestamp of previous buffer frame
								glUniform1f(glGetUniformLocation(glBufferShaderProgramIDs[i], iChannelTimeUniforms[k]), (GLfloat) scTimestamp);

								continue;
							}
						}
					}

					// Render Buffer i
					glBindVertexArray(glSquareVAO);
					glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
					glBindVertexArray(0);
					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					glFlush();
				}
			}
			
			// Render Main Shader
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, glWidth, glHeight);
			glClearColor(0, 0, 0, 0);
			glClear(GL_COLOR_BUFFER_BIT);

			glUseProgram(glMainShaderProgramID);

			// Load all Basic inputs
			glUniform3f(glGetUniformLocation(glMainShaderProgramID, "iResolution"), (float) glWidth, (float) glHeight, 0.0);
			glUniform1f(glGetUniformLocation(glMainShaderProgramID, "iTime"), (float) glfwGetTime());
			glUniform1f(glGetUniformLocation(glMainShaderProgramID, "iTimeDelta"), (float) (glfwGetTime() - scTimestamp));
			glUniform1i(glGetUniformLocation(glMainShaderProgramID, "iFrame"), scFrames);

			// TODO: Validate iMouse.zw data
			if (scMouseEnabled)
				glUniform4f(glGetUniformLocation(glMainShaderProgramID, "iMouse"), (GLfloat) currentMouse.x, (GLfloat) currentMouse.y, (GLfloat) scMouse.x, (GLfloat) scMouse.y);
			else
				glUniform4f(glGetUniformLocation(glMainShaderProgramID, "iMouse"), (GLfloat) scMouse.x, (GLfloat) scMouse.y, 0, 0);

			glUniform4f(glGetUniformLocation(glMainShaderProgramID, "iDate"), (GLfloat) iDate_year, (GLfloat) iDate_month, (GLfloat) iDate_day, (GLfloat) iDate_time);

			// Default value for SampleRate
			// TODO: Should evaluate from inputs
			glUniform1f(glGetUniformLocation(glMainShaderProgramID, "iSampleRate"), 0);

			// Bind iChannel data
			for (int k = 0; k < 4; ++k) {
				if (scMainShaderInputs[k] == -1) {
					glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
					glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
					glUniform1i(glGetUniformLocation(glMainShaderProgramID, iChannelUniforms[k]), 0); // GL_TEXTURE0 which is unused
					continue;
				}

				if (scResources[scMainShaderInputs[k]].empty) {
					std::wcout << "Can not configure iChannelResolution for input " << k << " in Main Shader, input points to empty resource, scResources corrupt" << std::endl;
					glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
					glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
					glUniform1i(glGetUniformLocation(glMainShaderProgramID, iChannelUniforms[k]), 0); // GL_TEXTURE0 which is unused
					continue;
				}

				switch (scResources[scMainShaderInputs[k]].resource.type) {
					case IMAGE_TEXTURE: {

						// Bind texture
						// TODO: Support for Sampler3D (requires shader recompile), e.t.c.
						glActiveTexture(GL_TEXTURE1 + k);
						glBindTexture(GL_TEXTURE_2D, scResources[scMainShaderInputs[k]].resource.bind);
						glUniform1i(glGetUniformLocation(glMainShaderProgramID, iChannelUniforms[k]), 1 + k);

						// Width & Height 
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) scResources[scMainShaderInputs[k]].resource.width, (GLfloat) scResources[scMainShaderInputs[k]].resource.height, (GLfloat) 0);

						// Timestamp 0
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);

						continue;
					}

					case AUDIO_TEXTURE: { // TODO: Compute input dimensions
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case VIDEO_TEXTURE: {
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case MIC_TEXTURE: {
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case WEB_TEXTURE: {
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case KEYBOARD_TEXTURE: {
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) 0, (GLfloat) 0, (GLfloat) 0);
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) 0);
						continue;
					}

					case FRAME_BUFFER: { // Buffer size always match the viewport size

						// Bind texture
						glActiveTexture(GL_TEXTURE1 + k);
						glBindTexture(GL_TEXTURE_2D, glBufferShaderFramebufferTextures[scBufferFrames[scResources[scMainShaderInputs[k]].resource.buffer_id] & 1][scResources[scMainShaderInputs[k]].resource.buffer_id]);
						glUniform1i(glGetUniformLocation(glMainShaderProgramID, iChannelUniforms[k]), 1 + k);

						// Width & Height 
						glUniform3f(glGetUniformLocation(glMainShaderProgramID, iChannelResolutionUniforms[k]), (GLfloat) glWidth, (GLfloat) glHeight, (GLfloat) 0);

						// Timestamp of previous buffer frame
						glUniform1f(glGetUniformLocation(glMainShaderProgramID, iChannelTimeUniforms[k]), (GLfloat) scTimestamp);

						continue;
					}
				}
			}

			// Render Main Shader
			glBindVertexArray(glSquareVAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);

			glFlush();
			SwapBuffers(glDevice);

			// Update required values
			scTimestamp = (float) glfwGetTime();
			++scFrames;

			// Tick framebuffer frames count only if framebuffer shader was active
			for (int i = 0; i < 4; ++i)
				if (glBufferShaderShouldBeRendered[i] && glBufferShaderProgramIDs[i] != -1)
					++scBufferFrames[i];

			// Update mouse location
			scMouse.x = currentMouse.x;
			scMouse.y = currentMouse.y;
		}
	}
}


// Starts thread for rendering scene, synchronizes with main thread for careful event processing and resource rebinding
void startRenderThread() {
	renderThread = new std::thread([] () {

		// Just timers
		std::chrono::system_clock::time_point a = std::chrono::system_clock::now();
		std::chrono::system_clock::time_point b = std::chrono::system_clock::now();

		// Lock motex to avoid interruption when reloading resources or exiting
		renderMutex.lock();
		
		// Acquire context
		wglMakeCurrent(glDevice, glContext);

		// Yes, while true, i don't care
		while (true) {

			// To prevent mutex lock / unlock 100500 times a second, check if lock was requested by simply 
			//  cheking bool and after that try to relock
			if (appLockRequested) {

				// if lock was requested, release GL context
				wglMakeCurrent(NULL, NULL);

				// Unlock mutex and allow main thread to load texture / rebind shader / e.t.c
				renderMutex.unlock();
				// appLockRequested = FALSE; // TODO: Main thread should clear this flag manually
				
				// Wait 50ms else it will run into undefined race condilion self-lock
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				
				// Now main thread acquires lock
				renderMutex.lock();

				// Acquire context back
				wglMakeCurrent(glDevice, glContext);
			}

			// Check if render exit was requested
			if (appExiting) {

				// Stop render process and thread
				wglMakeCurrent(NULL, NULL);
				renderMutex.unlock();
				return;

			} else if (scPaused) {
				std::this_thread::sleep_for(std::chrono::milliseconds(scMinFrameTime));
			} else{
				a = std::chrono::system_clock::now();

				// One frame render
				static PAINTSTRUCT ps;
				BeginPaint(glWindow, &ps);
				renderSC();
				EndPaint(glWindow, &ps);

				b = std::chrono::system_clock::now();
				auto work_time = std::chrono::duration_cast<std::chrono::milliseconds>(a - b);

				if (work_time.count() < scMinFrameTime)
					std::this_thread::sleep_for(std::chrono::milliseconds(scMinFrameTime - work_time.count()));
			}
		}
	});
}


// Dispose all resources
void dispose() {
	
	wglMakeCurrent(glDevice, glContext);

	// Unlink all shaders & buffers
	unloadMainShader();

	for (int i = 0; i < 4; ++i) 
		if (glBufferShaderProgramIDs[i] != -1)
			glDeleteProgram(glBufferShaderProgramIDs[i]);

	// Buffer i buffer & texture
	glDeleteTextures(4, glBufferShaderFramebufferTextures[0]);
	glDeleteTextures(4, glBufferShaderFramebufferTextures[1]);
	glDeleteBuffers(4, glBufferShaderFramebuffers[0]);
	glDeleteBuffers(4, glBufferShaderFramebuffers[1]);

	// Square buffer
	glDeleteVertexArrays(1, &glSquareVAO);
	glDeleteBuffers(1, &glSquareVBO);
	glDeleteBuffers(1, &glSquareEBO);

	// Unlink all resources
	unloadResources();

	wglMakeCurrent(NULL, NULL);

	if (trayMainMenu != 0)
		DestroyMenu(trayMainMenu);
}


// Carefull stop application with dispose of all resources, threads and exit
void exitApp() {

	// Wait for rendering thread to exit
	appExiting = TRUE;
	if (renderThread) {
		renderThread->join();
		delete renderThread;
	}

	// Tray icon delete
	Shell_NotifyIcon(NIM_DELETE, &trayNID);
	
	// TODO: Menu dispose?
	// ...

	// Resource dispose
	dispose();

	// Windows dispose (rm -rf /)
	wglMakeCurrent(NULL, NULL);
	ReleaseDC(glWindow, glDevice);
	wglDeleteContext(glContext);
	DestroyWindow(trayWindow);
	DestroyWindow(glWindow);

	if (glPalette)
		DeleteObject(glPalette);

	// Repaint desktop window
	repaintDesktop();
}


// Handler for enumerating all existing displays
BOOL CALLBACK displayEnumerateProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	MONITORINFO info;
	info.cbSize = sizeof(info);
	if (GetMonitorInfo(hMonitor, &info))
		rawDisplays.push_back(info.rcMonitor);

	return TRUE;
}

// Enumerates all existying displays in the system to save their size into global variables
void enumerateDisplays() {
	rawDisplays.clear();
	displays.clear();

	// Obsessive compulsive disorder wants me to zero it
	fullViewportSize.left = 0;
	fullViewportSize.right = 0;
	fullViewportSize.top = 0;
	fullViewportSize.bottom = 0;

	EnumDisplayMonitors(NULL, NULL, displayEnumerateProc, 0);

	// Find minimal coordinates
	int min_x = 0, min_y = 0;

	for (size_t i = 0; i < rawDisplays.size(); ++i) {
		fullViewportSize.left   = min(fullViewportSize.left,   rawDisplays[i].left);
		fullViewportSize.top    = min(fullViewportSize.top,    rawDisplays[i].top);
		fullViewportSize.right  = max(fullViewportSize.right,  rawDisplays[i].right);
		fullViewportSize.bottom = max(fullViewportSize.bottom, rawDisplays[i].bottom);

		min_x = min(rawDisplays[i].left, min_x);
		min_y = min(rawDisplays[i].top, min_y);
	}

	// Get resulting full viewport width & height
	fullViewportSize.right  -= fullViewportSize.left;
	fullViewportSize.bottom -= fullViewportSize.top;
	fullViewportSize.left = 0;
	fullViewportSize.top = 0;

	// Calculate absolute dimensions for displays
	displays.resize(rawDisplays.size());
	for (size_t i = 0; i < rawDisplays.size(); ++i) {
		int delta_x = rawDisplays[i].left - min_x;
		int delta_y = rawDisplays[i].top - min_y;

		displays[i].left   = rawDisplays[i].left   - min_x;
		displays[i].top    = rawDisplays[i].top    - min_y;
		displays[i].right  = rawDisplays[i].right  - min_x;
		displays[i].bottom = rawDisplays[i].bottom - min_y;
	}
}


// Tray window event dispatcher
// TODO: Accelerators &
LRESULT CALLBACK trayWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { 

	// Event specific info
	int wmId, wmEvent;

	// Tray click position
	POINT lpClickPoint;

	switch (uMsg) {
		case WM_TRAY_ICON: {
			switch (LOWORD(lParam)) {
				case WM_RBUTTONDOWN: {
					// Retrieve cursor position and spawn menu in click location
					GetCursorPos(&lpClickPoint);

					// Free old menu
					if (trayMainMenu != 0) {
						DestroyMenu(trayMainMenu);
						trayMenuHandlers.clear();
					}

					int menuId = TRAY_MENU_BASE_ID;
					int disabledId = TRAY_MENU_BASE_DISABLED_ID;
					trayMainMenu = CreatePopupMenu();

					HMENU displaySelectionMenu = CreatePopupMenu();
					
					for (int displayId = 0; displayId < displays.size(); ++displayId) {

						std::wstring displayDesc = std::wstring(L"Display ") + std::to_wstring(displayId) + L" (" + std::to_wstring(displays[displayId].right - displays[displayId].left) + L"x" + std::to_wstring(displays[displayId].bottom - displays[displayId].top) + L")";
						
						InsertMenu(displaySelectionMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, displayDesc.c_str());
						trayMenuHandlers.push_back([displayId]() {

							// Save me from a little duck constantly watching me
							appLockRequested = TRUE;
							renderMutex.lock();

							// Rescan displays because user may reconnect them
							enumerateDisplays();

							// Shift display
							scDisplayID = displayId;

							// Check if there is > 0 displays
							if (displays.size() == 0) {

								std::wcout << "Display not found: Can not display shader decause no displays were found on the system" << std::endl;
								MessageBoxA(
									NULL,
									"Can not display shader decause no displays found on the system",
									"Display not found",
									MB_ICONERROR | MB_OK
								);

								appLockRequested = FALSE;
								renderMutex.unlock();

								return;
							}

							// Ensure no out of bounds, loop displays
							if (scDisplayID >= displays.size())
								scDisplayID = 0;

							currentWindowDimensions = displays[scDisplayID];

							// Window size & location
							MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

							// GL size
							glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
							glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

							wglMakeCurrent(glDevice, glContext);
							resizeSC();
							wglMakeCurrent(NULL, NULL);

							// Update flag (obviously)
							scFullscreen = FALSE;

							// Request repaint after display changed
							repaintDesktop();

							appLockRequested = FALSE;
							renderMutex.unlock();
						});
						if (scDisplayID == displayId)
							CheckMenuItem(displaySelectionMenu, menuId - 1, MF_CHECKED);
					}

					// Obviously, build menu
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) displaySelectionMenu, (std::wstring(L"Primary Display: ") + std::to_wstring(scDisplayID) + L" (" + std::to_wstring(displays[scDisplayID].right - displays[scDisplayID].left) + L"x" + std::to_wstring(displays[scDisplayID].bottom - displays[scDisplayID].top) + L")").c_str());
					
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId, _T("Fullscreen"));
					trayMenuHandlers.push_back([]() {
						appLockRequested = TRUE;
						renderMutex.lock();

						if (scFullscreen) {

							// Rescan displays because user may reconnect them
							enumerateDisplays();

							// Check if there is > 0 displays
							if (displays.size() == 0) {

								std::wcout << "Display not found: Can not display shader decause no displays were found on the system" << std::endl;
								MessageBoxA(
									NULL,
									"Can not display shader decause no displays found on the system",
									"Display not found",
									MB_ICONERROR | MB_OK
								);

								appLockRequested = FALSE;
								renderMutex.unlock();

								return;
							}

							// Ensure no out of bounds
							if (scDisplayID >= displays.size())
								scDisplayID = (int) displays.size() - 1;

							currentWindowDimensions = displays[scDisplayID];

							// Window size & location
							MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

							// GL size
							glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
							glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

							wglMakeCurrent(glDevice, glContext);
							resizeSC();
							wglMakeCurrent(NULL, NULL);

							// Update flag (obviously)
							scFullscreen = FALSE;

						} else {

							// Rescan displays because user may reconnect them
							enumerateDisplays();

							// Check if there is > 0 displays
							if (displays.size() == 0) {

								std::wcout << "Display not found: Can not display shader decause no displays were found on the system" << std::endl;
								MessageBoxA(
									NULL,
									"Can not display shader decause no displays found on the system",
									"Display not found",
									MB_ICONERROR | MB_OK
								);

								appLockRequested = FALSE;
								renderMutex.unlock();

								return;
							}

							currentWindowDimensions = fullViewportSize;

							// Window size & location
							MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

							// GL size
							glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
							glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

							wglMakeCurrent(glDevice, glContext);
							resizeSC();
							wglMakeCurrent(NULL, NULL);

							// Update flag (obviously)
							scFullscreen = TRUE;
						}

						// Request repaint after display changed
						repaintDesktop();

						appLockRequested = FALSE;
						renderMutex.unlock();
					});
					if (scFullscreen)
						CheckMenuItem(trayMainMenu, menuId, MF_CHECKED);
					++menuId;
					
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Rescan displays"));
					trayMenuHandlers.push_back([]() {
						appLockRequested = TRUE;
						renderMutex.lock();

						// TODO: Push window to WorkerW if it was reset (explorer.exe restart)
						// Rescan displays because user may reconnect them
						enumerateDisplays();

						// Check if there is > 0 displays
						if (displays.size() == 0) {

							std::wcout << "Display not found: Can not display shader decause no displays found on the system" << std::endl;
							MessageBoxA(
								NULL,
								"Can not display shader decause no displays found on the system",
								"Display not found",
								MB_ICONERROR | MB_OK
							);

							appLockRequested = FALSE;
							renderMutex.unlock();

							return;
						}

						if (scFullscreen) {

							currentWindowDimensions = fullViewportSize;

							// Window size & location
							MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

							// GL size
							int nglWidth = currentWindowDimensions.right - currentWindowDimensions.left;
							int nglHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

							// Resize context and textures only if there is a size mismatch
							if (nglWidth != glWidth || nglHeight != glHeight) {
								glWidth = nglWidth;
								glHeight = nglHeight;

								wglMakeCurrent(glDevice, glContext);
								resizeSC();
								wglMakeCurrent(NULL, NULL);
							}

						} else {

							// Ensure no out of bounds, loop displays
							if (scDisplayID >= displays.size())
								scDisplayID = 0;

							currentWindowDimensions = displays[scDisplayID];

							// Window size & location
							MoveWindow(glWindow, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, TRUE);

							// GL size
							int nglWidth = currentWindowDimensions.right - currentWindowDimensions.left;
							int nglHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

							// Resize context and textures only if there is a size mismatch
							if (nglWidth != glWidth || nglHeight != glHeight) {
								glWidth = nglWidth;
								glHeight = nglHeight;

								wglMakeCurrent(glDevice, glContext);
								resizeSC();
								wglMakeCurrent(NULL, NULL);
							}
						}

						appLockRequested = FALSE;
						renderMutex.unlock();
					});

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId, _T("Pause"));
					trayMenuHandlers.push_back([]() {
						if (scPaused) {
							appLockRequested = TRUE;
							renderMutex.lock();

							// Restore timestamp
							wglMakeCurrent(glDevice, glContext);
							glfwSetTime(scTimestamp);
							wglMakeCurrent(NULL, NULL);

							appLockRequested = FALSE;
							renderMutex.unlock();
						}

						scPaused = !scPaused;
					});
					if (scPaused)
						CheckMenuItem(trayMainMenu, menuId, MF_CHECKED);
					++menuId;

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Reset time"));
					trayMenuHandlers.push_back([]() {
						appLockRequested = TRUE;
						renderMutex.lock();

						wglMakeCurrent(glDevice, glContext);

						// Clear buffers
						for (int i = 0; i < 4; ++i) {

							// First
							glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[0][i]);
							glViewport(0, 0, glWidth, glHeight);
							glClearColor(0, 0, 0, 0);
							glClear(GL_COLOR_BUFFER_BIT);

							// Second
							glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[1][i]);
							glViewport(0, 0, glWidth, glHeight);
							glClearColor(0, 0, 0, 0);
							glClear(GL_COLOR_BUFFER_BIT);

							glBindFramebuffer(GL_FRAMEBUFFER, 0);

							scBufferFrames[i] = 0;
						}

						// Reset time & frame
						glfwSetTime(0.0);
						scTimestamp = 0.0;
						scFrames = 0;
						// for (int i = 0; i < 4; ++i)
						// 	++scBufferFrames[i];

						wglMakeCurrent(NULL, NULL);

						appLockRequested = FALSE;
						renderMutex.unlock();
					});

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Reset buffers"));
					trayMenuHandlers.push_back([]() {

						appLockRequested = TRUE;
						renderMutex.lock();
						wglMakeCurrent(glDevice, glContext);

						// Bind each buffer and do glClearColor
						for (int i = 0; i < 4; ++i) {
							glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[0][i]);
							glViewport(0, 0, glWidth, glHeight);
							glClearColor(0, 0, 0, 0);
							glClear(GL_COLOR_BUFFER_BIT);

							glBindFramebuffer(GL_FRAMEBUFFER, glBufferShaderFramebuffers[1][i]);
							glViewport(0, 0, glWidth, glHeight);
							glClearColor(0, 0, 0, 0);
							glClear(GL_COLOR_BUFFER_BIT);

							glBindFramebuffer(GL_FRAMEBUFFER, 0);

							scBufferFrames[i] = 0;
						}

						wglMakeCurrent(NULL, NULL);
						appLockRequested = FALSE;
						renderMutex.unlock();
					});

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Reload inputs"));
					trayMenuHandlers.push_back([]() {
						appLockRequested = TRUE;
						renderMutex.lock();
						wglMakeCurrent(glDevice, glContext);

						reloadResources();

						wglMakeCurrent(NULL, NULL);
						appLockRequested = FALSE;
						renderMutex.unlock();
					});

					bool inputs_exist = false;
					for (int i = 0; i < 4; ++i) 
						if (scMainShaderInputs[i] != -1) {
							inputs_exist = true;
							break;
						}

					if (!inputs_exist)
						for (int k = 0; k < 4; ++k)
							for (int i = 0; i < 4; ++i) {
								if (scBufferShaderInputs[i][k] != -1) {
									inputs_exist = true;
									break;
								}

								if (inputs_exist)
									break;
							}
					
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Clear all inputs"));
					trayMenuHandlers.push_back([]() {

						appLockRequested = TRUE;
						renderMutex.lock();
						wglMakeCurrent(glDevice, glContext);

						unloadResources();

						wglMakeCurrent(NULL, NULL);
						appLockRequested = FALSE;
						renderMutex.unlock();
					});
					if (!inputs_exist)
						EnableMenuItem(trayMainMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Reload shaders"));
					trayMenuHandlers.push_back([]() {

						appLockRequested = TRUE;
						renderMutex.lock();
						wglMakeCurrent(glDevice, glContext);

						reloadMainShader();
						reloadBufferShader(0);
						reloadBufferShader(1);
						reloadBufferShader(2);
						reloadBufferShader(3);

						wglMakeCurrent(NULL, NULL);
						appLockRequested = FALSE;
						renderMutex.unlock();
					});

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					HMENU trayFPSSelectMenu = CreatePopupMenu();
					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("1 FPS"));
					trayMenuHandlers.push_back([]() {
						scMinFrameTime = 1000;
						scFPSMode = 1;
					});

					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("15 FPS"));
					trayMenuHandlers.push_back([]() {
						scMinFrameTime = 1000 / 15;
						scFPSMode = 15;
					});

					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("30 FPS"));
					trayMenuHandlers.push_back([]() {
						scMinFrameTime = 1000 / 30;
						scFPSMode = 30;
					});

					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("60 FPS"));
					trayMenuHandlers.push_back([]() {
						scMinFrameTime = 1000 / 60;
						scFPSMode = 60;
					});

					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("120 FPS"));
					trayMenuHandlers.push_back([]() {
						scMinFrameTime = 1000 / 120;
						scFPSMode = 120;
					});

					InsertMenu(trayFPSSelectMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("240 FPS"));
					trayMenuHandlers.push_back([]() {
						scMinFrameTime = 1000 / 240;
						scFPSMode = 240;
					});

					// IDK how to concat and use this LPWSTRPTR shit
					if (scFPSMode == 1) {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (1)"));
						CheckMenuItem(trayFPSSelectMenu, menuId - 6, MF_CHECKED);
					} else if (scFPSMode == 15) {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (15)"));
						CheckMenuItem(trayFPSSelectMenu, menuId - 5, MF_CHECKED);
					} else if (scFPSMode == 30) {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (30)"));
						CheckMenuItem(trayFPSSelectMenu, menuId - 4, MF_CHECKED);
					} else if (scFPSMode == 60) {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (60)"));
						CheckMenuItem(trayFPSSelectMenu, menuId - 3, MF_CHECKED);
					} else if (scFPSMode == 120) {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (120)"));
						CheckMenuItem(trayFPSSelectMenu, menuId - 2, MF_CHECKED);
					} else { // scFPSMode == 240
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayFPSSelectMenu, _T("FPS (240)"));
						CheckMenuItem(trayFPSSelectMenu, menuId - 1, MF_CHECKED);
					}

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					// TODO: Sound input
					/*
					
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId, _T("Enable sound capture"));
					trayMenuHandlers.push_back([]() {
						appLockRequested = TRUE;
						renderMutex.lock();

						scSoundEnabled = !scSoundEnabled;

						appLockRequested = FALSE;
						renderMutex.unlock();
					});
					if (scSoundEnabled)
						CheckMenuItem(trayMainMenu, menuId, MF_CHECKED);
					++menuId;

					// TODO: Enumerate sound inputs as submenu items + add selection mark
					HMENU traySoundSourceMenu = CreatePopupMenu();

					const wchar_t* sources[3] = {
						L"Low definition audio device",
						L"Micwofone", 
						L"Moan box"
					};

					int sourceSelectionId = 2;

					for (int index = 0; index < 3; ++index) {
						InsertMenu(traySoundSourceMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId, sources[index]);
						trayMenuHandlers.push_back([index]() {
							std::wcout << "Incomplete :: Sound source sub :: " << index << std::endl;
						});
						if (index == sourceSelectionId)
							CheckMenuItem(traySoundSourceMenu, menuId, MF_CHECKED);
						++menuId;
					}

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) traySoundSourceMenu, _T("Sound source"));

					*/

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId, _T("Enable mouse"));
					trayMenuHandlers.push_back([]() {
						appLockRequested = TRUE;
						renderMutex.lock();

						scMouseEnabled = !scMouseEnabled;

						appLockRequested = FALSE;
						renderMutex.unlock();
					});
					if (scMouseEnabled)
						CheckMenuItem(trayMainMenu, menuId, MF_CHECKED);
					++menuId;

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					// Show short pack path
					if (scPackPath != L"") {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, disabledId++, (L"Opened: " + std::filesystem::path(scPackPath).filename().wstring()).c_str());
						EnableMenuItem(trayMainMenu, disabledId - 1, MF_DISABLED | MF_GRAYED); // Disabled

						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Close pack"));
						trayMenuHandlers.push_back([]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							// Unload everything
							unloadMainShader();

							for (int i = 0; i < 4; ++i)
								if (glBufferShaderProgramIDs[i] != -1)
									unloadBufferShader(i);

							// Clear framebuffer
							glBindFramebuffer(GL_FRAMEBUFFER, 0);
							glViewport(0, 0, glWidth, glHeight);
							glClearColor(0, 0, 0, 0);
							glClear(GL_COLOR_BUFFER_BIT);

							unloadResources();

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();

							scPackPath = L"";
						});

						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					}

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Open pack"));
					trayMenuHandlers.push_back([]() {

						COMDLG_FILTERSPEC fileTypes[] = {
							{ L"JSON files", L"*.json" },
							{ L"Any files", L"*" }
						};

						std::wstring packPath = openFile(ARRAYSIZE(fileTypes), fileTypes);

						if (packPath.size() != 0) {

							scPackPath = packPath;

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);
							scPaused = TRUE;

							reloadPack();

							scPaused = FALSE;
							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						}
					});

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Reload pack"));
					trayMenuHandlers.push_back([]() {
						if (scPackPath.size() != 0) {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);
							scPaused = TRUE;

							reloadPack();

							scPaused = FALSE;
							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						}
					});
					if (scPackPath == L"")
						EnableMenuItem(trayMainMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("New pack"));
					trayMenuHandlers.push_back([]() {

						COMDLG_FILTERSPEC fileTypes[] = {
							{ L"JSON files", L"*.json" },
							{ L"Any files", L"*" }
						};

						std::wstring packPath = saveFile(ARRAYSIZE(fileTypes), fileTypes, L"pack.json");

						// Only set path for this pack and do not create files
						if (packPath != L"")
							scPackPath = packPath;
					});

					// Save pack
					trayMenuHandlers.push_back([]() {

						COMDLG_FILTERSPEC fileTypes[] = {
							{ L"JSON files", L"*.json" },
							{ L"Any files", L"*" }
						};

						std::wstring packPath = saveFile(ARRAYSIZE(fileTypes), fileTypes, L"pack.json");

						// Obviously, get filename and save
						if (packPath != L"") {
							scPackPath = packPath;

							// Warning: This will save shader files as Main.glsl, Buffer[A/B/C/D].glsl without overwrite prompt
							savePack();
						}
					});

					if (scPackPath == L"") 
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Save pack")); // Prompt and save, same handler
					else {
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId + 1, _T("Save pack")); // Save to scPackPath
						trayMenuHandlers.push_back([]() {

							// Obviously, save
							// Warning: This will save shader files as Main.glsl, Buffer[A/B/C/D].glsl without overwrite prompt
							if (scPackPath != L"")
								savePack();
						});

						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId, _T("Save pack as")); // Prompt and save, same handler

						menuId += 2;
					}

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					// Create menu for Main Shader

					HMENU trayMainShaderMenu = CreatePopupMenu();

					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Open"));
					trayMenuHandlers.push_back([]() {

						COMDLG_FILTERSPEC fileTypes[] = {
							{ L"GLSL files", L"*.glsl" },
							{ L"Any files", L"*" }
						};

						std::wstring shaderPath = openFile(ARRAYSIZE(fileTypes), fileTypes);

						appLockRequested = TRUE;
						renderMutex.lock();
						wglMakeCurrent(glDevice, glContext);

						if (shaderPath.size() != 0) 
							loadMainShaderFromFile(shaderPath);

						wglMakeCurrent(NULL, NULL);
						appLockRequested = FALSE;
						renderMutex.unlock();
					});

					if (glMainShaderPath != L"") {
						InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Reload"));
						trayMenuHandlers.push_back([]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							reloadMainShader();

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});
					}

					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("New"));
					trayMenuHandlers.push_back([]() {

						COMDLG_FILTERSPEC fileTypes[] = {
							{ L"GLSL files", L"*.glsl" },
							{ L"Any files", L"*" }
						};

						std::wstring shaderPath = saveFile(ARRAYSIZE(fileTypes), fileTypes, L"Main.glsl");

						appLockRequested = TRUE;
						renderMutex.lock();
						wglMakeCurrent(glDevice, glContext);

						if (shaderPath.size() != 0) {

							// Default Main shader
							std::ofstream out(shaderPath.c_str());
							if (!out) {
								std::wcout << "New :: Failed to create Main Shader file :: " << shaderPath << std::endl;

								MessageBox(
									NULL,
									(L"Failed to create Main Shader file " + shaderPath).c_str(),
									L"Failed to create file",
									MB_ICONERROR | MB_OK
								);

								wglMakeCurrent(NULL, NULL);
								appLockRequested = FALSE;
								renderMutex.unlock();
								return;
							}
							out << defaultMainShader;
							out.close();

							// Obviously, open this shader
							loadMainShaderFromFile(shaderPath);
						}

						wglMakeCurrent(NULL, NULL);
						appLockRequested = FALSE;
						renderMutex.unlock();
					});

					// Display only if at least one input is used
					InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Clear inputs"));
					trayMenuHandlers.push_back([]() {

						appLockRequested = TRUE;
						renderMutex.lock();
						wglMakeCurrent(glDevice, glContext);

						for (int i = 0; i < 4; ++i)
							unloadMainShaderResource(i);

						wglMakeCurrent(NULL, NULL);
						appLockRequested = FALSE;
						renderMutex.unlock();
					});

					for (int i = 0; i < 4; ++i) {
						if (scMainShaderInputs[i] != -1)
							break;

						if (i == 3)
							EnableMenuItem(trayMainShaderMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
					}
					
					for (int inputId = 0; inputId < 4; ++inputId) {

						HMENU trayMainInputTypeMenu = CreatePopupMenu();

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("None"));
						trayMenuHandlers.push_back([inputId]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							unloadMainShaderResource(inputId);

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Buffer A"));
						trayMenuHandlers.push_back([inputId]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							SCResource input;
							input.type = FRAME_BUFFER;
							input.buffer_id = 0;

							loadMainShaderResource(input, inputId);

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Buffer B"));
						trayMenuHandlers.push_back([inputId]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							SCResource input;
							input.type = FRAME_BUFFER;
							input.buffer_id = 1;

							loadMainShaderResource(input, inputId);

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Buffer C"));
						trayMenuHandlers.push_back([inputId]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							SCResource input;
							input.type = FRAME_BUFFER;
							input.buffer_id = 2;

							loadMainShaderResource(input, inputId);

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Buffer D"));
						trayMenuHandlers.push_back([inputId]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							SCResource input;
							input.type = FRAME_BUFFER;
							input.buffer_id = 3;

							loadMainShaderResource(input, inputId);

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Image"));
						trayMenuHandlers.push_back([inputId]() {

							COMDLG_FILTERSPEC fileTypes[] = {
								{ L"PNG images", L"*.png" }
							};

							std::wstring imagePath = openFile(ARRAYSIZE(fileTypes), fileTypes);

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							if (imagePath.size() != 0) {

								SCResource input;
								input.type = IMAGE_TEXTURE;
								input.path = imagePath;

								loadMainShaderResource(input, inputId);
							}

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});

						// TODO: Support other input types
						/*
						
						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Keyboard"));
						EnableMenuItem(trayMainInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
						trayMenuHandlers.push_back([inputId]() {
							std::wcout << "Incomplete :: Main Shader :: Set input " << inputId << " :: Keyboard" << std::endl;
						});

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Microphone"));
						EnableMenuItem(trayMainInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
						trayMenuHandlers.push_back([inputId]() {
							std::wcout << "Incomplete :: Main Shader :: Set input " << inputId << " :: Nicrophone" << std::endl;
						});

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Audio file"));
						EnableMenuItem(trayMainInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
						trayMenuHandlers.push_back([inputId]() {
							std::wcout << "Incomplete :: Main Shader :: Set input " << inputId << " :: Audio" << std::endl;
						});

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Video file"));
						EnableMenuItem(trayMainInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
						trayMenuHandlers.push_back([inputId]() {
							std::wcout << "Incomplete :: Main Shader :: Set input " << inputId << " :: Video" << std::endl;
						});

						InsertMenu(trayMainInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Webcam"));
						EnableMenuItem(trayMainInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
						trayMenuHandlers.push_back([inputId]() {
							std::wcout << "Incomplete :: Main Shader :: Set input " << inputId << " :: Webcam" << std::endl;
						});

						*/


						if (scMainShaderInputs[inputId] == -1 || scResources[scMainShaderInputs[inputId]].empty)
							InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, (std::wstring(L"Input ") + std::to_wstring(inputId)).c_str());
						else
							InsertMenu(trayMainShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainInputTypeMenu, (std::wstring(L"Input ") + std::to_wstring(inputId) + L": " + resourceToShordDescription(scResources[scMainShaderInputs[inputId]].resource)).c_str());
					}

					// Insert this menu, yes
					if (glMainShaderPath != L"")
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainShaderMenu, (L"Main shader: " + std::filesystem::path(glMainShaderPath).filename().wstring()).c_str());
					else
						InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayMainShaderMenu, L"Main shader");
					
					// Create menu for each Buffer Shader
					for (int bufferId = 0; bufferId < 4; ++bufferId) {
						
						HMENU trayBufferShaderMenu = CreatePopupMenu();

						InsertMenu(trayBufferShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Open"));
						trayMenuHandlers.push_back([bufferId]() {

							COMDLG_FILTERSPEC fileTypes[] = {
								{ L"GLSL files", L"*.glsl" },
								{ L"Any files", L"*" }
							};

							std::wstring shaderPath = openFile(ARRAYSIZE(fileTypes), fileTypes);

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							if (shaderPath.size() != 0)
								loadBufferShaderFromFile(shaderPath, bufferId);

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});

						InsertMenu(trayBufferShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Reload"));
						trayMenuHandlers.push_back([bufferId]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							reloadBufferShader(bufferId);

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});
						if (glBufferShaderPath[bufferId] == L"")
							EnableMenuItem(trayBufferShaderMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled

						InsertMenu(trayBufferShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("New"));
						trayMenuHandlers.push_back([bufferId]() {

							COMDLG_FILTERSPEC fileTypes[] = {
								{ L"GLSL files", L"*.glsl" },
								{ L"Any files", L"*" }
							};

							const wchar_t* fileNames[] = {
								L"BufferA.glsl",
								L"BufferB.glsl",
								L"BufferC.glsl",
								L"BufferD.glsl"
							};

							const std::wstring bufferNames[] = {
								L"BufferA",
								L"BufferB",
								L"BufferC",
								L"BufferD"
							};

							std::wstring shaderPath = saveFile(ARRAYSIZE(fileTypes), fileTypes, fileNames[bufferId]);

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							if (shaderPath.size() != 0) {

								// Default Buffer shader
								std::ofstream out(shaderPath.c_str());
								if (!out) {
									std::wcout << "New :: Failed to create " << bufferNames[bufferId] << " Shader file :: " << shaderPath << std::endl;

									MessageBox(
										NULL,
										(L"Failed to create " + bufferNames[bufferId] + L" Shader file " + shaderPath).c_str(),
										L"Failed to create file",
										MB_ICONERROR | MB_OK
									);

									wglMakeCurrent(NULL, NULL);
									appLockRequested = FALSE;
									renderMutex.unlock();
									return;
								}
								out << defaultBufferShader;
								out.close();

								// Obviously, open this shader
								loadBufferShaderFromFile(shaderPath, bufferId);
							}

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});
						
						InsertMenu(trayBufferShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Remove")); // TODO: Change to "Enabled" and "Paused"? because if it is paused, it is not evaluated on each frame and if it is not enabled, it is removed from pack
						trayMenuHandlers.push_back([bufferId]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							unloadBufferShader(bufferId);

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});
						if (glBufferShaderPath[bufferId] == L"")
							EnableMenuItem(trayBufferShaderMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled

						// Display only if at least one input is used
						InsertMenu(trayBufferShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Clear inputs"));
						trayMenuHandlers.push_back([bufferId]() {

							appLockRequested = TRUE;
							renderMutex.lock();
							wglMakeCurrent(glDevice, glContext);

							for (int i = 0; i < 4; ++i)
								unloadBufferShaderResource(bufferId, i);

							wglMakeCurrent(NULL, NULL);
							appLockRequested = FALSE;
							renderMutex.unlock();
						});

						for (int i = 0; i < 4; ++i) {
							if (scBufferShaderInputs[bufferId][i] != -1)
								break;
							
							if (i == 3)
								EnableMenuItem(trayBufferShaderMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
						}

						for (int inputId = 0; inputId < 4; ++inputId) {

							HMENU trayBufferInputTypeMenu = CreatePopupMenu();

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("None"));
							trayMenuHandlers.push_back([bufferId, inputId]() {

								appLockRequested = TRUE;
								renderMutex.lock();
								wglMakeCurrent(glDevice, glContext);

								unloadBufferShaderResource(bufferId, inputId);

								wglMakeCurrent(NULL, NULL);
								appLockRequested = FALSE;
								renderMutex.unlock();
							});

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Buffer A"));
							trayMenuHandlers.push_back([bufferId, inputId]() {

								appLockRequested = TRUE;
								renderMutex.lock();
								wglMakeCurrent(glDevice, glContext);

								SCResource input;
								input.type = FRAME_BUFFER;
								input.buffer_id = 0;

								loadBufferShaderResource(input, bufferId, inputId);

								wglMakeCurrent(NULL, NULL);
								appLockRequested = FALSE;
								renderMutex.unlock();
							});

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Buffer B"));
							trayMenuHandlers.push_back([bufferId, inputId]() {

								appLockRequested = TRUE;
								renderMutex.lock();
								wglMakeCurrent(glDevice, glContext);

								SCResource input;
								input.type = FRAME_BUFFER;
								input.buffer_id = 1;

								loadBufferShaderResource(input, bufferId, inputId);

								wglMakeCurrent(NULL, NULL);
								appLockRequested = FALSE;
								renderMutex.unlock();
							});

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Buffer C"));
							trayMenuHandlers.push_back([bufferId, inputId]() {

								appLockRequested = TRUE;
								renderMutex.lock();
								wglMakeCurrent(glDevice, glContext);

								SCResource input;
								input.type = FRAME_BUFFER;
								input.buffer_id = 2;

								loadBufferShaderResource(input, bufferId, inputId);

								wglMakeCurrent(NULL, NULL);
								appLockRequested = FALSE;
								renderMutex.unlock();
							});

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Buffer D"));
							trayMenuHandlers.push_back([bufferId, inputId]() {

								appLockRequested = TRUE;
								renderMutex.lock();
								wglMakeCurrent(glDevice, glContext);

								SCResource input;
								input.type = FRAME_BUFFER;
								input.buffer_id = 3;

								loadBufferShaderResource(input, bufferId, inputId);

								wglMakeCurrent(NULL, NULL);
								appLockRequested = FALSE;
								renderMutex.unlock();
							});

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Image"));
							trayMenuHandlers.push_back([bufferId, inputId]() {

								COMDLG_FILTERSPEC fileTypes[] = {
									{ L"PNG images", L"*.png" }
								};

								std::wstring imagePath = openFile(ARRAYSIZE(fileTypes), fileTypes);

								appLockRequested = TRUE;
								renderMutex.lock();
								wglMakeCurrent(glDevice, glContext);

								if (imagePath.size() != 0) {

									SCResource input;
									input.type = IMAGE_TEXTURE;
									input.path = imagePath;

									loadBufferShaderResource(input, bufferId, inputId);
								}

								wglMakeCurrent(NULL, NULL);
								appLockRequested = FALSE;
								renderMutex.unlock();
							});


							// TODO: Support other input types
							/*

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Keyboard"));
							EnableMenuItem(trayBufferInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
							trayMenuHandlers.push_back([bufferId, inputId]() {
								std::wcout << "Incomplete :: Buffer " << bufferId << " :: Set input " << inputId << " :: Keyboard" << std::endl;
							});

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Microphone"));
							EnableMenuItem(trayBufferInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
							trayMenuHandlers.push_back([bufferId, inputId]() {
								std::wcout << "Incomplete :: Buffer " << bufferId << " :: Set input " << inputId << " :: Microphone" << std::endl;
							});

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Audio file"));
							EnableMenuItem(trayBufferInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
							trayMenuHandlers.push_back([bufferId, inputId]() {
								std::wcout << "Incomplete :: Buffer " << bufferId << " :: Set input " << inputId << " :: Audio" << std::endl;
							});

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Video file"));
							EnableMenuItem(trayBufferInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
							trayMenuHandlers.push_back([bufferId, inputId]() {
								std::wcout << "Incomplete :: Buffer " << bufferId << " :: Set input " << inputId << " :: Video" << std::endl;
							});

							InsertMenu(trayBufferInputTypeMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Webcam"));
							EnableMenuItem(trayBufferInputTypeMenu, menuId - 1, MF_DISABLED | MF_GRAYED); // Disabled
							trayMenuHandlers.push_back([bufferId, inputId]() {
								std::wcout << "Incomplete :: Buffer " << bufferId << " :: Set input " << inputId << " :: Webcam" << std::endl;
							});

							*/

							if (scBufferShaderInputs[bufferId][inputId] == -1 || scResources[scBufferShaderInputs[bufferId][inputId]].empty)
								InsertMenu(trayBufferShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayBufferInputTypeMenu, (std::wstring(L"Input ") + std::to_wstring(inputId)).c_str());
							else 
								InsertMenu(trayBufferShaderMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayBufferInputTypeMenu, (std::wstring(L"Input ") + std::to_wstring(inputId) + L": " + resourceToShordDescription(scResources[scBufferShaderInputs[bufferId][inputId]].resource)).c_str());
						}

						const wchar_t* menuEntry[4] = {
							L"Buffer A Shader",
							L"Buffer B Shader",
							L"Buffer C Shader",
							L"Buffer D Shader"
						};

						const wchar_t* menuEntryColon[4] = {
							L"Buffer A Shader: ",
							L"Buffer B Shader: ",
							L"Buffer C Shader: ",
							L"Buffer D Shader: "
						};

						// Insert that menu, no
						if (glBufferShaderPath[bufferId] != L"")
							InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayBufferShaderMenu, (menuEntryColon[bufferId] + std::filesystem::path(glBufferShaderPath[bufferId]).filename().wstring()).c_str());
						else
							InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING | MF_POPUP, (UINT_PTR) trayBufferShaderMenu, menuEntry[bufferId]);
					}

					//
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_SEPARATOR, IDM_SEP, _T("SEP"));
					//

					// Display version
					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, disabledId++, (std::wstring(L"Vebro Version: ") + VERSION).c_str());
					EnableMenuItem(trayMainMenu, disabledId - 1, MF_DISABLED | MF_GRAYED); // Disabled

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId, _T("Enable debug console"));
					trayMenuHandlers.push_back([]() {
						if (!wndDebugOutput) {
							if (AllocConsole()) {
								SetConsoleTitle(L"Debug output");
								SetConsoleCtrlHandler(NULL, TRUE);
								FILE* _frp = freopen("CONOUT$", "w", stdout);
							}
						} else {
							ShowWindow(GetConsoleWindow(), SW_HIDE);
							FreeConsole();
						}
						wndDebugOutput = !wndDebugOutput;
					});
					if (wndDebugOutput)
						CheckMenuItem(trayMainMenu, menuId, MF_CHECKED);
					++menuId;

					InsertMenu(trayMainMenu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, menuId++, _T("Exit"));
					trayMenuHandlers.push_back([]() {
						PostQuitMessage(0);
					});

					// Display menu, finally
					SetForegroundWindow(hWnd);
					TrackPopupMenu(trayMainMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, lpClickPoint.x, lpClickPoint.y, 0, hWnd, NULL);
					return TRUE;
				}

				default:
					return DefWindowProc(hWnd, uMsg, wParam, lParam);
			}
			break;
		}
		
		case WM_COMMAND: {
			wmId = LOWORD(wParam);
			wmEvent = HIWORD(wParam);

			// Validates bounds (If there will be more than one menu in the future, idk)
			if (wmId >= TRAY_MENU_BASE_ID) {

				int handlerId = wmId - TRAY_MENU_BASE_ID;

				// Hey I just met you
				// And this is crazy
				// But here's my handle
				// So call me maybe
				if (handlerId >= 0 && handlerId < trayMenuHandlers.size()) {
					trayMenuHandlers[handlerId]();
					return TRUE;
				}
			}

			return DefWindowProc(hWnd, wmId, wParam, lParam);
		}
		
		case WM_DESTROY: {
			PostQuitMessage(0);
			return TRUE;
		}
		
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Initializes the tray menu window and related
// Returns 0 on success, 1 else
int createTrayMenu(_In_ HINSTANCE hInstance) {
	// Register window instance
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC) trayWindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON));
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = L"MenuName";
	wcex.lpszClassName = L"SHADERWALLPAPER_Tray";

	if (!RegisterClassEx(&wcex)) {
		std::wcout << "Failed to register [SHADERWALLPAPER_TRAY] window class" << std::endl;
		return 1;
	}

	// Initialize window
	trayWindow = CreateWindow(L"SHADERWALLPAPER_Tray", L"Shader Wallpaper", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
	if (!trayWindow) {
		std::wcout << "Failed to create window with class [SHADERWALLPAPER_Tray]" << std::endl;
		return 1;
	}

	// Load icon
	trayIcon = LoadIcon(hInstance, (LPCTSTR) MAKEINTRESOURCE(IDI_ICON));

	// Create Icon Data structure
	trayNID;
	trayNID.cbSize = sizeof(NOTIFYICONDATA);
	// Handle of the window which will process this app. messages 
	trayNID.hWnd = trayWindow;
	trayNID.uID = IDI_ICON;
	trayNID.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	trayNID.hIcon = trayIcon;
	trayNID.uCallbackMessage = WM_TRAY_ICON;
	std::wcscpy(trayNID.szTip, L"Vebro Menu");

	// Push icon to tray
	Shell_NotifyIcon(NIM_ADD, &trayNID);

	return 0;
}


// Desktop window event dispatcher
LRESULT CALLBACK desktopWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		//case WM_PAINT: {
		//	// Prevent ignoring of paint event because GL implicitly forces window to repaint
		//	return TRUE;
		//}

		case WM_SIZE: {
			// Do nothing, resized on menu item click inside mutex lock context
			return TRUE;
		}

		// IDK what it does, lol
		case WM_PALETTECHANGED:
			if (hWnd == (HWND) wParam)
				break;

		case WM_QUERYNEWPALETTE:
			if (glPalette) {
				UnrealizeObject(glPalette);
				SelectPalette(glDevice, glPalette, FALSE);
				RealizePalette(glDevice);
				return TRUE;
			}
			return TRUE;

		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Initializes the GL window with context and cookies
// Returns 0 on success, 1 else
int createDesktopWindow(_In_ HINSTANCE hInstance) {
	// Locate WorkerW
	HWND workerw = WorkerWEnumerator::enumerateForWorkerW();

	if (workerw == NULL) {
		std::wcout << "WorkerW enumeration failed" << std::endl;
		return 1;
	}

	// Initial display enumeration
	enumerateDisplays();

	// Set single display or fullscreen
	if (!scFullscreen) {
		currentWindowDimensions = displays[scDisplayID];
	} else {
		currentWindowDimensions = fullViewportSize;
	}

	// Creating Window for OpenGL context
	WNDCLASS wc;
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC) desktopWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"VEBRO_OpenGL";

	if (!RegisterClass(&wc)) {
		std::wcout << "Failed to register [VEBRO_OpenGL] window class" << std::endl;
		return 1;
	}

	// Initialize window
	glWindow = CreateWindow(L"VEBRO_OpenGL", L"Sample Text", WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, currentWindowDimensions.left, currentWindowDimensions.top, currentWindowDimensions.right - currentWindowDimensions.left, currentWindowDimensions.bottom - currentWindowDimensions.top, NULL, NULL, hInstance, NULL);
	
	if (!glWindow) {
		std::wcout << "Failed to create window with class [VEBRO_OpenGL]" << std::endl;
		return 1;
	}

	// Change window style to remove border nand decorations
	DWORD style = ::GetWindowLong(glWindow, GWL_STYLE);
	style &= ~WS_OVERLAPPEDWINDOW;
	style |= WS_POPUP;
	::SetWindowLong(glWindow, GWL_STYLE, style);

	glDevice = GetDC(glWindow);

	// Apply pixel format for the window
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(pfd));
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;// | PFD_DOUBLEBUFFER; // PFD_SUPPORT_COMPOSITION | 
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	//pfd.iLayerType = PFD_MAIN_PLANE;

	int pf = ChoosePixelFormat(glDevice, &pfd);
	if (pf == 0) {
		std::wcout << "Can not find a suitable pixel format" << std::endl;
		return 1;
	}

	if (SetPixelFormat(glDevice, pf, &pfd) == FALSE) {
		std::wcout << "Can not set specified pixel format" << std::endl;
		return 1;
	}

	// This cake is not real
	DescribePixelFormat(glDevice, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

	if (pfd.dwFlags & PFD_NEED_PALETTE || pfd.iPixelType == PFD_TYPE_COLORINDEX) {
		int n = 1 << pfd.cColorBits;
		if (n > 256) 
			n = 256;

		LOGPALETTE* lpPal = (LOGPALETTE*) malloc(sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * n);
		if (!lpPal) {
			std::wcout << "Failed to allocate palette" << std::endl;
			return 1;
		}

		memset(lpPal, 0, sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * n);
		lpPal->palVersion = 0x300;
		lpPal->palNumEntries = n;

		GetSystemPaletteEntries(glDevice, 0, n, &lpPal->palPalEntry[0]);

		if (pfd.iPixelType == PFD_TYPE_RGBA) {
			int redMask   = (1 << pfd.cRedBits)   - 1;
			int greenMask = (1 << pfd.cGreenBits) - 1;
			int blueMask  = (1 << pfd.cBlueBits)  - 1;
			int i;

			for (i = 0; i < n; ++i) {
				lpPal->palPalEntry[i].peRed   = (((i >> pfd.cRedShift) & redMask) * 255) / redMask;
				lpPal->palPalEntry[i].peGreen = (((i >> pfd.cGreenShift) & greenMask) * 255) / greenMask;
				lpPal->palPalEntry[i].peBlue  = (((i >> pfd.cBlueShift) & blueMask) * 255) / blueMask;
				lpPal->palPalEntry[i].peFlags = 0;
			}
		}
		else {
			lpPal->palPalEntry[0].peRed   = 0;
			lpPal->palPalEntry[0].peGreen = 0;
			lpPal->palPalEntry[0].peBlue  = 0;
			lpPal->palPalEntry[0].peFlags = PC_NOCOLLAPSE;
			lpPal->palPalEntry[1].peRed   = 255;
			lpPal->palPalEntry[1].peGreen = 0;
			lpPal->palPalEntry[1].peBlue  = 0;
			lpPal->palPalEntry[1].peFlags = PC_NOCOLLAPSE;
			lpPal->palPalEntry[2].peRed   = 0;
			lpPal->palPalEntry[2].peGreen = 255;
			lpPal->palPalEntry[2].peBlue  = 0;
			lpPal->palPalEntry[2].peFlags = PC_NOCOLLAPSE;
			lpPal->palPalEntry[3].peRed   = 0;
			lpPal->palPalEntry[3].peGreen = 0;
			lpPal->palPalEntry[3].peBlue  = 255;
			lpPal->palPalEntry[3].peFlags = PC_NOCOLLAPSE;
		}

		glPalette = CreatePalette(lpPal);
		if (glPalette) {
			SelectPalette(glDevice, glPalette, FALSE);
			RealizePalette(glDevice);
		}

		free(lpPal);
	}

	// Create GL context & init
	glContext = wglCreateContext(glDevice);
	wglMakeCurrent(glDevice, glContext);

	if (!glfwInit()) {
		std::wcout << "Failed to initialize GLFW" << std::endl;
		return 1;
	}

	glewExperimental = GL_TRUE;
	if (GLEW_OK != glewInit()) {
		std::wcout << "Failed to initialize GLEW" << std::endl;
		return 1;
	}

	glWidth = currentWindowDimensions.right - currentWindowDimensions.left;
	glHeight = currentWindowDimensions.bottom - currentWindowDimensions.top;

	// Pass window to the WorkerW
	ReleaseDC(glWindow, glDevice);
	SetParent(glWindow, workerw);

	return 0;
}


// Message dispatcher simply handles all WINAPI messages and do it's work
void enterDispatchLoop() {

	MSG msg;

	while (1) {
		// Block till message received
		if (GetMessage(&msg, NULL, 0, 0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else
			break;

		if (msg.message == WM_QUIT)
			break;
	}
}


// Command line options parsing
// https://stackoverflow.com/a/868894
// Modified: returns index of arguemnt or 0 (zero argument is always program name)
size_t getCmdOptionIndex(wchar_t** begin, wchar_t** end, const std::wstring& option) {
	wchar_t** itr = std::find(begin, end, option);
	if (itr != end) {
		std::wcout << itr - begin << std::endl;
		return itr - begin;
	}
	return 0;
}

bool cmdOptionExists(wchar_t** begin, wchar_t** end, const std::wstring& option) {
	return std::find(begin, end, option) != end;
}


// Entry
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow) {

#ifdef DEBUG_DISPLAY_CONSOLE_WINDOW
	
	if (AllocConsole()) {
		SetConsoleTitle(L"Debug output");
		SetConsoleCtrlHandler(NULL, TRUE);
		FILE* _frp = freopen("CONOUT$", "w", stdout);
	}

#endif

	bool useDebugConsole = cmdOptionExists(__wargv, __wargv + __argc, L"--debug");

	// Open console first for errors tracking
	if (useDebugConsole) {

		FreeConsole();

		if (AllocConsole()) {
			SetConsoleTitle(L"Debug output");
			SetConsoleCtrlHandler(NULL, TRUE);
			FILE* _frp = freopen("CONOUT$", "w", stdout);
		}
	} 
#ifndef DEBUG_DISPLAY_CONSOLE_WINDOW

	else {
		if (AttachConsole(ATTACH_PARENT_PROCESS)) {
			FILE* _frp = freopen("CONOUT$", "w", stdout);
		}
	}

#endif

	// Change output mode to Unicode text
	int _sm = _setmode(_fileno(stdout), _O_U16TEXT);

	
	// Parse first portion of arguments

	// Help message
	if (cmdOptionExists(__wargv, __wargv + __argc, L"-h") || cmdOptionExists(__wargv, __wargv + __argc, L"--help")) {
		std::wcout << "help for commandline options (use separately)" << std::endl;
		std::wcout << " -h, --help         display help" << std::endl;

		// Display properties
		std::wcout << " --display <id>     default display ID (>= 0)" << std::endl;
		std::wcout << " --fullscreen       enable fullscreen mode" << std::endl; // Overwrite displayID

		// FPS properties
		std::wcout << " --fps <fps>        set fps (1-240)" << std::endl;

		// Input properties
		std::wcout << " --mouse            enable mouse input" << std::endl;

		// Pack selection
		std::wcout << " --pack             pack json location" << std::endl;

		// Shader properties (Override pack)
		std::wcout << " --main             main shader location" << std::endl;
		std::wcout << " --main:0           main shader Input 0 (type:path), exmaple: image:shrek.png" << std::endl;
		std::wcout << " --main:1           main shader Input 1 (type:path)" << std::endl;
		std::wcout << " --main:2           main shader Input 2 (type:path)" << std::endl;
		std::wcout << " --main:3           main shader Input 3 (type:path)" << std::endl;

		std::wcout << " --a                Buffer A shader location" << std::endl;
		std::wcout << " --a:0              Buffer A Input 0 (type:path)" << std::endl;
		std::wcout << " --a:1              Buffer A Input 1 (type:path)" << std::endl;
		std::wcout << " --a:2              Buffer A Input 2 (type:path)" << std::endl;
		std::wcout << " --a:3              Buffer A Input 3 (type:path)" << std::endl;

		std::wcout << " --b                Buffer B shader location" << std::endl;
		std::wcout << " --b:0              Buffer B Input 0 (type:path)" << std::endl;
		std::wcout << " --b:1              Buffer B Input 1 (type:path)" << std::endl;
		std::wcout << " --b:2              Buffer B Input 2 (type:path)" << std::endl;
		std::wcout << " --b:3              Buffer B Input 3 (type:path)" << std::endl;

		std::wcout << " --c                Buffer C shader location" << std::endl;
		std::wcout << " --c:0              Buffer C Input 0 (type:path)" << std::endl;
		std::wcout << " --c:1              Buffer C Input 1 (type:path)" << std::endl;
		std::wcout << " --c:2              Buffer C Input 2 (type:path)" << std::endl;
		std::wcout << " --c:3              Buffer C Input 3 (type:path)" << std::endl;

		std::wcout << " --d                Buffer D shader location" << std::endl;
		std::wcout << " --d:0              Buffer D Input 0 (type:path)" << std::endl;
		std::wcout << " --d:1              Buffer D Input 1 (type:path)" << std::endl;
		std::wcout << " --d:2              Buffer D Input 2 (type:path)" << std::endl;
		std::wcout << " --d:3              Buffer D Input 3 (type:path)" << std::endl;

		// Debug properties
		std::wcout << " --debug            enable debug output" << std::endl;

		// DEBUG:
		// system("PAUSE");

		if (useDebugConsole)
			system("PAUSE");

		return 0;
	}

	// Index of argument
	size_t argi = 0;

	// Display ID
	if (argi = getCmdOptionIndex(__wargv, __wargv + __argc, L"--display")) {
		if (argi + 1 >= __argc) {
			std::wcout << "Expected displayID argument" << std::endl;

			if (useDebugConsole)
				system("PAUSE");

			exit(0);
		}

		try {
			scDisplayID = std::stoi(__wargv[argi + 1]);
		} catch (...) {
			std::wcout << "Expected displayID argument" << std::endl;

			if (useDebugConsole)
				system("PAUSE");

			exit(0);
		}
	}

	// Fullscreen
	scFullscreen = cmdOptionExists(__wargv, __wargv + __argc, L"--fullscreen");

	// FPS
	if (argi = getCmdOptionIndex(__wargv, __wargv + __argc, L"--fps")) {
		if (argi + 1 >= __argc) {
			std::wcout << "Expected FPS argument" << std::endl;

			if (useDebugConsole)
				system("PAUSE");

			exit(0);
		}

		try {
			scFPSMode = std::stoi(__wargv[argi + 1]);
		} catch (...) {
			std::wcout << "Expected FPS argument" << std::endl;

			if (useDebugConsole)
				system("PAUSE");

			exit(0);
		}

		if (scFPSMode <= 0 || scFPSMode > 240) {
			std::wcout << "FPS out of range (1-240)" << std::endl;

			if (useDebugConsole)
				system("PAUSE");

			exit(0);
		}

		scMinFrameTime = 1000 / scFPSMode;
	}

	// Moise input
	scMouseEnabled = cmdOptionExists(__wargv, __wargv + __argc, L"--mouse");


	// Create Windows & GL Context

	// Create Tray menu
	if (createTrayMenu(hInstance)) {
		std::wcout << "Failed initialization of Tray Menu window" << std::endl;
		system("PAUSE");
		return 0;
	}

	// Create window on desktop for wallpaper
	if (createDesktopWindow(hInstance)) {
		std::wcout << "Failed initialization of GL context or Worker window" << std::endl;
		system("PAUSE");
		return 0;
	}

	// Initialize render context
	wglMakeCurrent(glDevice, glContext);

	initSC();

	ShowWindow(glWindow, nCmdShow);
	UpdateWindow(glWindow);

	// Release OpenGL context for this thread
	wglMakeCurrent(NULL, NULL);

	// Acquire GL context
	wglMakeCurrent(glDevice, glContext);


	// Parse rest of arguments (Textures, inputs)

	// Pack path
	if (argi = getCmdOptionIndex(__wargv, __wargv + __argc, L"--pack")) {
		if (argi + 1 >= __argc) {
			std::wcout << "Expected pack path argument" << std::endl;

			if (useDebugConsole)
				system("PAUSE");

			exitApp(); exit(0);
		}

		try {
			std::wcout << __argc << L" " << argi << L" " << (argi + 1) << std::endl;
			std::wcout << L"pack: " << __wargv[argi + 1] << std::endl;
			scPackPath = std::filesystem::absolute(__wargv[argi + 1]);
			std::wcout << L"scPackPath: " << scPackPath << std::endl;

			reloadPack();
		} catch (...) {
			std::wcout << "Pack not found in " << __wargv[argi + 1] << std::endl;

			if (useDebugConsole)
				system("PAUSE");

			exitApp(); exit(0);
		}
	}

	// Main shader & options
	if (argi = getCmdOptionIndex(__wargv, __wargv + __argc, L"--main")) {
		if (argi + 1 >= __argc) {
			std::wcout << "Expected main shader path argument" << std::endl;

			if (useDebugConsole)
				system("PAUSE");

			exitApp(); exit(0);
		}

		try {
			std::wstring shaderPath = std::filesystem::absolute(__wargv[argi + 1]);

			loadMainShaderFromFile(shaderPath);
		} catch (...) {
			std::wcout << "Main shader not found in " << __wargv[argi + 1] << std::endl;

			if (useDebugConsole)
				system("PAUSE");

			exitApp(); exit(0);
		}
	}

	for (int inputId = 0; inputId < 4; ++inputId) {
		if (argi = getCmdOptionIndex(__wargv, __wargv + __argc, std::wstring(L"--main:") + std::to_wstring(inputId))) {
			if (argi + 1 >= __argc) {
				std::wcout << "Expected main shader input " << inputId << " argument" << std::endl;

				if (useDebugConsole)
					system("PAUSE");

				exitApp(); exit(0);
			}

			std::wstring arg = __wargv[argi];
			
			if (arg.rfind(L"image:", 0) == 0) {
				try {
					std::wstring imagePath = std::filesystem::absolute(arg.substr(6)); // XXX: hardcoded constant

					SCResource input;
					input.type = IMAGE_TEXTURE;
					input.path = imagePath;

					loadMainShaderResource(input, inputId);
				} catch (...) {
					std::wcout << "Main shader input " << inputId << " image not found in " << __wargv[argi + 1] << std::endl;

					if (useDebugConsole)
						system("PAUSE");

					exitApp(); exit(0);
				}
			} else if (arg == L"a") {
				SCResource input;
				input.type = FRAME_BUFFER;
				input.buffer_id = 0;

				loadMainShaderResource(input, inputId);
			} else if (arg == L"b") {
				SCResource input;
				input.type = FRAME_BUFFER;
				input.buffer_id = 1;

				loadMainShaderResource(input, inputId);
			} else if (arg == L"c") {
				SCResource input;
				input.type = FRAME_BUFFER;
				input.buffer_id = 2;

				loadMainShaderResource(input, inputId);
			} else if (arg == L"d") {
				SCResource input;
				input.type = FRAME_BUFFER;
				input.buffer_id = 3;

				loadMainShaderResource(input, inputId);
			} else if (arg == L"none") {
				unloadMainShaderResource(inputId);
			} else {
				std::wcout << "Main shader input " << inputId << " has unsupported value " << arg << ", expected one of : (image:path, a, b, c, d, none)" << __wargv[argi + 1] << std::endl;

				if (useDebugConsole)
					system("PAUSE");

				exitApp(); exit(0);
			}
		}
	}

	// Buffer shaders & options
	for (int bufferId = 0; bufferId < 4; ++bufferId) {
		wchar_t buffer_label = L"abcd"[bufferId];

		if (argi = getCmdOptionIndex(__wargv, __wargv + __argc, std::wstring(L"--") + buffer_label)) {
			if (argi + 1 >= __argc) {
				std::wcout << "Expected buffer " << buffer_label << " shader path argument" << std::endl;

				if (useDebugConsole)
					system("PAUSE");

				exitApp(); exit(0);
			}

			try {
				std::wstring shaderPath = std::filesystem::absolute(__wargv[argi + 1]);

				loadBufferShaderFromFile(shaderPath, bufferId);
			} catch (...) {
				std::wcout << "Buffer " << buffer_label << " shader not found in " << __wargv[argi + 1] << std::endl;

				if (useDebugConsole)
					system("PAUSE");

				exitApp(); exit(0);
			}
		}

		for (int inputId = 0; inputId < 4; ++inputId) {
			if (argi = getCmdOptionIndex(__wargv, __wargv + __argc, std::wstring(L"--") + buffer_label + L":" + std::to_wstring(inputId))) {
				if (argi + 1 >= __argc) {
					std::wcout << "Expected buffer " << buffer_label << " shader input " << inputId << " argument" << std::endl;

					if (useDebugConsole)
						system("PAUSE");

					exitApp(); exit(0);
				}

				std::wstring arg = __wargv[argi];

				if (arg.rfind(L"image:", 0) == 0) {
					try {
						std::wstring imagePath = std::filesystem::absolute(arg.substr(6)); // XXX: hardcoded constant

						SCResource input;
						input.type = IMAGE_TEXTURE;
						input.path = imagePath;

						loadBufferShaderResource(input, bufferId, inputId);
					} catch (...) {
						std::wcout << "Buffer " << buffer_label << " shader input " << inputId << " image not found in " << __wargv[argi + 1] << std::endl;

						if (useDebugConsole)
							system("PAUSE");

						exitApp();
					}
				} else if (arg == L"a") {
					SCResource input;
					input.type = FRAME_BUFFER;
					input.buffer_id = 0;

					loadMainShaderResource(input, inputId);
				} else if (arg == L"b") {
					SCResource input;
					input.type = FRAME_BUFFER;
					input.buffer_id = 1;

					loadMainShaderResource(input, inputId);
				} else if (arg == L"c") {
					SCResource input;
					input.type = FRAME_BUFFER;
					input.buffer_id = 2;

					loadMainShaderResource(input, inputId);
				} else if (arg == L"d") {
					SCResource input;
					input.type = FRAME_BUFFER;
					input.buffer_id = 3;

					loadMainShaderResource(input, inputId);
				} else if (arg == L"none") {
					unloadBufferShaderResource(bufferId, inputId);
				} else {
					std::wcout << "Buffer " << buffer_label << " shader input " << inputId << " has unsupported value " << arg << ", expected one of : (image:path, a, b, c, d, none)" << __wargv[argi + 1] << std::endl;

					if (useDebugConsole)
						system("PAUSE");

					exitApp(); exit(0);
				}
			}
		}
	}


	// Finally, start rendering if everything was ok
	// However..
	
	// Release GL context
	wglMakeCurrent(NULL, NULL);

	// Start new thread for render
	startRenderThread();

	// Enter message dispatching loop
	enterDispatchLoop();

	// After dispatch loop: exit and dispose
	exitApp();

	return 0;
}

