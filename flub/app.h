#ifndef _FLUB_APP_HEADER_
#define _FLUB_APP_HEADER_


#include <stdint.h>
#include <flub/cmdline_handler.h>
#include <physfs_memfile.h>


typedef struct flubResourceList_s {
    const char *name;
    PHYSFS_memfile *memfile;
} flubResourceList_t;


///////////////////////////////////////////////////////////////////////////////
///
/// \brief Flub application defaults structure.
///
/// This structure defines the application defaults for a program. The Flub
/// library uses this structure when initializing base subsystems and to
/// enable certain features.
///
///////////////////////////////////////////////////////////////////////////////
typedef struct appDefaults_s
{
    int major;  ///< Specifies the application's major version number
    int minor;  ///< Specifies the application's minor version number
    int argc;   ///< The number of command line parameters passed to the application
    char **argv;///< Array of strings of command line parameters passed to the application
    void *cmdlineContext;       ///< Pointer to application specific data structure for the command line parser
    const char *title;          ///< Specifies the application's title
    const char *configFile;     ///< Specifies the application's configuration variable file
    const char *bindingFile;    ///< Specifies the application's input binding file
    const char *archiveFile;    ///< Specifies the application's archive file
    const char *videoMode;      ///< Application's default video mode (ie., "640x480")
    const char *fullscreen;     ///< Boolean string that determines whether the app starts fullscreen
    int allowVideoModeChange;   ///< Specifies whether the video mode setting can be changed.
    int allowFullscreenChange;  ///< Specifies whether the fullscreen setting can be changed.
    cmdlineDefHandler_t cmdlineHandler; ///< Default command line parameter handler.
    const char *cmdlineParamStr;        ///< Command line help string suffix
    flubResourceList_t *resources; ///< NULL terminated list of the application's memfile resources
    size_t frameStackSize;      ///< Initial size of the reusable frame memory stack
} appDefaults_t;

///////////////////////////////////////////////////////////////////////////////
///
/// User application defined structure
///
/// Each application should define an appDefaults_t structure. This is a
/// reference to that global structure.
///
///////////////////////////////////////////////////////////////////////////////
extern appDefaults_t appDefaults;

///////////////////////////////////////////////////////////////////////////////
///
/// Check whether the application was launched from a console shell.
///
/// On Windows, this function attempts to determine if the application was
/// launched from an existing console shell. On all other platforms, this
/// simply returns true.
///
/// \return On Windows, returns true if the application was launched from an
///         existing console shell, otherwise false. All other platforms return
///         true.
///
///////////////////////////////////////////////////////////////////////////////
int appLaunchedFromConsole(void);

///////////////////////////////////////////////////////////////////////////////
///
/// Initializes the Flub library.
///
/// Brings up the underlying flub systems.
///
/// \param argc main()'s command line argument count variable
/// \param argv main()'s command line argument array pointer variable
/// \return True if Flub was initialized, false if Flub failed to initialize.
///
///////////////////////////////////////////////////////////////////////////////
int appInit(int argc, char **argv);

///////////////////////////////////////////////////////////////////////////////
///
/// Starts the Flub library.
///
/// Parses the command line, processes configuration variable settings,
/// sets the video mode, and prepares the application to run.
///
/// \param cmdlineContext Context pointer passed to the command line argument
///        handlers.
/// \return eCMDLINE_OK if the application is ready to run,
///         eCMDLINE_EXIT_SUCCESS if the application should exit successfully, or
///         eCMDLINE_EXIT_FAILURE if the application should exit due to a failure.
///
///////////////////////////////////////////////////////////////////////////////
eCmdLineStatus_t appStart(void *cmdlineContext);

///////////////////////////////////////////////////////////////////////////////
///
/// Shutdown the flub library.
///
/// Performs a controlled shutdown and cleanup of the flub sub-systems.
///
///////////////////////////////////////////////////////////////////////////////
void appShutdown(void);

///////////////////////////////////////////////////////////////////////////////
///
/// Updates Flub for a given frame.
///
/// \param ticks The global tick count for this frame
/// \return True if the application should continue running, false if Flub
///         wants to exit.
///
///////////////////////////////////////////////////////////////////////////////
int appUpdate(uint32_t ticks, uint32_t elapsed);

///////////////////////////////////////////////////////////////////////////////
///
/// The application's executable name, as called by the OS.
///
/// \return The application's executable name, without path.
///
///////////////////////////////////////////////////////////////////////////////
const char *appName(void);


#endif // _FLUB_APP_HEADER_
