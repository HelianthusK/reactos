#ifndef ROSBOOT_TEST_H__
#define ROSBOOT_TEST_H__

/* $Id$
 *
 * PROJECT:     System regression tool for ReactOS
 * LICENSE:     GPL - See COPYING in the top level directory
 * FILE:        tools/sysreg/conf_parser.h
 * PURPOSE:     ReactOS boot test
 * PROGRAMMERS: Johannes Anderwald (johannes.anderwald at sbox tugraz at)
 */
#include "data_source.h"
#include "conf_parser.h"
#include "os_support.h"
#include "user_types.h"
#include <vector>
#ifdef __LINUX__
#include <unistd.h>
#include <sys/types.h>
#if defined(__FreeBSD__) || defined(__APPLE__)
# include <sys/stat.h>
#endif // __FreeBSD__
#endif

namespace Sysreg_
{
	using std::vector;
    using System_::DataSource;
    using System_::OsSupport;


//---------------------------------------------------------------------------------------
///
/// class RosBootTest
///
/// Description: this class attempts to boot ReactOS in an emulator with console logging enabled.
/// It

	class RosBootTest
	{
	public:

		static string ROS_EMU_TYPE;
		static string EMU_TYPE_QEMU;
		static string EMU_TYPE_VMWARE;
		static string EMU_TYPE_XEN;
		static string ROS_EMU_PATH;
		static string ROS_HDD_IMAGE;
		static string ROS_CD_IMAGE;
		static string ROS_MAX_TIME;
		static string ROS_LOG_FILE;
		static string ROS_SYM_DIR;
		static string ROS_DELAY_READ;
		static string ROS_SYSREG_CHECKPOINT;
		static string ROS_CRITICAL_IMAGE;
		static string ROS_EMU_KILL;
		static string ROS_EMU_MEM;
		static string ROS_BOOT_CMD;
		static string XEN_CONFIG_FILE;
		static string XEN_CONFIG_NAME;

//---------------------------------------------------------------------------------------
///
/// RosBootTest
///
/// Description: constructor of class RosBootTest
///

		RosBootTest();

//---------------------------------------------------------------------------------------
///
/// ~RosBootTest
///
/// Description: destructor of class RosBootTest
///

		virtual ~RosBootTest();

//---------------------------------------------------------------------------------------
///
/// execute
///
/// Description: this function performs a ReactOS boot test. It reads the variable
/// ROSBOOT_CMD and executes this specific command. This command shall contain the path
/// to an emulator (i.e. qemu) and the required arguments (-serial switch, hdd img etc) to be able
/// to read from console. If an error is detected, it attempts to resolve the faulting
/// module and address.

	virtual bool execute(ConfigParser & conf_parser);

	protected:

    void getDefaultHDDImage(string & img);
    bool isFileExisting(string filename);
    bool isDefaultHDDImageExisting();
    bool createHDDImage(string filename);
    bool isQemuPathValid();
    bool getQemuDir(string &);
    bool createBootCmd();
    bool extractPipeFromBootCmd();
    bool configureCDImage();
    bool configureHDDImage();
    void getPidFromFile();
    bool executeBootCmd();
    void delayRead();
    bool configurePipe();
    bool configureFile();
    bool analyzeDebugData();
    bool readConfigurationValues(ConfigParser & conf_parser);
    bool configureQemu();
    bool configureVmWare();
    bool configureXen(ConfigParser &conf_parser);
    bool hasQemuNoRebootOption();
    void cleanup(ConfigParser &conf_parser);
    bool xenGetCaps();
//---------------------------------------------------------------------------------------
///
/// dumpCheckpoints
///
/// Description: prints a list of all reached checkpoints so far

	void dumpCheckpoints();

enum DebugState
{
	DebugStateContinue = 1, /* continue debugging */
	DebugStateBSODDetected, /* bsod detected */
	DebugStateUMEDetected, /* user-mode exception detected */
	DebugStateCPReached /* check-point reached */
};

//---------------------------------------------------------------------------------------
///
/// checkDebugData
///
/// Description: this function parses the given debug data for BSOD, UM exception etc
///              If it detects an fatal error, it should return false
///
/// Note: the received debug information should be written to an internal log object
/// to facilate post-processing of the results

	DebugState checkDebugData(vector<string> & debug_data);

protected:

    string m_EmuType;
    string m_EmuPath;
    string m_HDDImage;
    string m_CDImage;
    long int m_MaxTime;
    string m_DebugFile;
    string m_SymDir;
    long int m_DelayRead;
    string m_Checkpoint;
    string m_CriticalImage;
    string m_KillEmulator;
    string m_MaxMem;
    string m_BootCmd;
    string m_Src;
    string m_DebugPort;
    string m_PidFile;
	string m_XenConfig;

    DataSource * m_DataSource;
    OsSupport::ProcessID m_Pid;
    std::vector<string> m_Checkpoints;

    }; // end of class RosBootTest

} // end of namespace Sysreg_

#endif /* end of ROSBOOT_H__ */
