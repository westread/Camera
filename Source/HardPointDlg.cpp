
// HardPointDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "HardPoint.h"
#include "HardPointDlg.h"
#include "afxdialogex.h"
#include "tool/CommUtil.h"
#include "dtutil.h"
#include "tool/dttool.h"
#include "Protocol/TheUtil.h"
#include <pylon/PylonIncludes.h>
#include "opencv/opencv2/opencv.hpp"
#include "CommonFunc.h"

#include <fstream>
using namespace std;
using namespace Pylon;
using namespace GenApi;
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
// Namespace for using pylon objects.

const int TIMER_CHECK_LOG = 2003;
const int TIMER_UPDATE_DATA = 2004;

CCheckDevices::CCheckDevices(CHardPointDlg *pDlg)
{
	m_pDlg = pDlg;
}

CCheckDevices::~CCheckDevices()
{
	CloseThreadTask();
}

DWORD CCheckDevices::Process()
{
	CAPPLog::Log(LOG_INFO, "[INFO]CCheckDevices::Process->enter");
	SetTimeOutMicroSecond(200);
	while (true)
	{
		//m_pDlg->CheckDevices();
		m_pDlg->SendToServer();
		if (Wait() == TASK_STOP)
		{
			break;
		}
	}
	CAPPLog::Log(LOG_INFO, "[INFO]CCheckDevices::Process->leave");
	return 0;
}
//保存硬点图片线程
CSaveImage::CSaveImage(CHardPointDlg *pDlg)
{
	m_pDlg = pDlg;
	m_pImgQueue = new CMySimpleQueue<S_IMAGE_DATA *>(1,m_pDlg->GetArySize());
}
CSaveImage::~CSaveImage()
{
	CloseThreadTask();
	if (m_pImgQueue)
	{
		delete m_pImgQueue;
		m_pImgQueue = NULL;
	}
}
void CSaveImage::AddImage(S_IMAGE_DATA *pImg)
{
	m_pImgQueue->PutAnItem(*pImg);
}
DWORD CSaveImage::Process()
{
	CAPPLog::Log(LOG_INFO, "[INFO]CSaveImage::Process->enter");
	SetTimeOutMicroSecond(200);
	while (true)
	{
		if (m_pImgQueue->GetCount() > 0)
		{
			S_IMAGE_DATA data;
			m_pImgQueue->GetAnItem(data,0,true);
			cv::Mat MatImg(data.iWidth, data.iHeight, CV_8UC1, data.pImg);
			TCHAR szTmp[MAX_PATH];
			m_pDlg->GetDataPath(szTmp);
			//目录+文件名
			TCHAR fileName[MAX_PATH];
			_stprintf(fileName, _T("%s\\%s.jpg"), szTmp, data.strTime.substr(8).c_str());
			char *psz = NULL;
			int iLen = 0;
			DTCT2A(&psz, iLen, fileName);
			cv::imwrite(psz, MatImg);
			delete[]psz;
			psz = NULL;

			if (data.pImg)
			{
				delete data.pImg;
				data.pImg = NULL;
			}
		}

		if (Wait() == TASK_STOP)
		{
			break;
		}
	}
	CAPPLog::Log(LOG_INFO, "[INFO]CSaveImage::Process->leave");
	return 0;
}

CGrabImage::CGrabImage(CHardPointDlg *pDlg)
{
	m_pDlg = pDlg;
}

CGrabImage::~CGrabImage()
{
	CloseThreadTask();
}

DWORD CGrabImage::Process()
{
	CAPPLog::Log(LOG_INFO, "[INFO]CGrabImage::Process->enter");
	SetTimeOutMicroSecond(2);
	while (true)
	{
		if (!m_pDlg->GrabImage()) break;
		if (Wait() == TASK_STOP)
		{
			break;
		}
	}
	CAPPLog::Log(LOG_INFO, "[INFO]CGrabImage::Process->leave");
	return 0;
}

CFTPFile::CFTPFile(CHardPointDlg *pDlg)
{
	m_pDlg = pDlg;
}

CFTPFile::~CFTPFile()
{
	CloseThreadTask();
}

DWORD CFTPFile::Process()
{
	CAPPLog::Log(LOG_INFO, "[INFO]CFTPFile::Process->enter");
	SetTimeOutMicroSecond(1000);
	int iCount = 0;
	while (true)
	{
		m_pDlg->FtpFile();
		if (Wait() == TASK_STOP)
		{
			break;
		}
	}
	CAPPLog::Log(LOG_INFO, "[INFO]CFTPFile::Process->leave");
	return 0;
}

// CHardPointDlg 对话框
CHardPointDlg::CHardPointDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CHardPointDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	_tcscpy(m_szAppPath, _T(""));
	_tcscpy(m_szIniFile, _T(""));
	GetAppPath(m_szAppPath, MAX_PATH);
	TCHAR szLogFile[MAX_PATH];
	SYSTEMTIME lt;
	GetLocalTime(&lt);
	_stprintf(szLogFile, _T("%sLog\\"), m_szAppPath);
	TestandCreateDirectory(szLogFile);
	_stprintf(szLogFile, _T("%s%04d%02d%02d.txt"), szLogFile, lt.wYear, lt.wMonth, lt.wDay);
	char *pszLog = NULL;
	int iPath = 0;
	DTCT2A(&pszLog, iPath, szLogFile);
	CAPPLog::SetLogFile(pszLog);
	CAPPLog::SetMaxLogFileSize(20000);
	delete[]pszLog;
	_tcscpy(m_szIniFile, m_szAppPath);
	_tcscat(m_szIniFile, _T("SystemConfig.ini"));
	CAPPLog::Log(LOG_FORCE, _T("[FORCE]------------a new process:%s"), m_szAppPath);

	m_sServerIP.clear();
	m_iServerPort = 0;
	m_pClient = NULL;
	m_bRunning = false;
	m_fYD = 0;
	m_fYD_YZ = 0;
	m_pCmdQueue = NULL;
	m_bConnect = false;
	m_pCheckThread = NULL;
	for (int j = 0; j < 3; j++)
		m_arrCheckTimes[j] = 0;
	m_pCamera = NULL;
	m_pGrabThread = NULL;
	m_iHZ = 100; 
	m_aryTail = 0;
	m_iIsCalc = 0;
	m_iEqual = 0;

	m_pFtpFileQueue = new CMySimpleStringQueue(2, 1000);
	//m_pFtpFileQueue->PutAnItem("E:\\GrabData\\Datas\\20190417\\0001\\02\\04\\W20190417.txt");
	m_pFtpFileThread = NULL;
	m_sftpDate = _T("00000000");

}

CHardPointDlg::~CHardPointDlg()
{
	if (m_pCheckThread!=NULL)
	{
		delete m_pCheckThread;
		m_pCheckThread = NULL;
	}
	if (m_pFtpFileThread != NULL)
	{
		delete m_pFtpFileThread;
		m_pFtpFileThread = NULL;
	}

	if (m_pClient != NULL)
	{
		delete m_pClient;
		m_pClient = NULL;
	}
	if (m_pCmdQueue != NULL)
	{
		delete m_pCmdQueue;
		m_pCmdQueue = NULL;
	}

	if (m_pFtpFileQueue != NULL)
	{
		delete m_pFtpFileQueue;
		m_pFtpFileQueue = NULL;
	}

	CloseCamera();

	ReleaseBuffer();
}

void CHardPointDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CHardPointDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_BN_CLICKED(ID_BT_START, &CHardPointDlg::OnBnClickedBtStart)
END_MESSAGE_MAP()


// CHardPointDlg 消息处理程序

BOOL CHardPointDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	ReadConfig();

	TCHAR str[12];
	_stprintf(str, _T("%6.1f"), m_fYD_YZ);
	GetDlgItem(IDC_EDIT_YDYZ)->SetWindowTextW(str);

	InitBuffer();

	TCHAR szStatus[64];

	if (!m_sServerIP.empty() && m_iServerPort > 0)
	{
		CAPPLog::Log(LOG_INFO, _T("OnInitDialog->Server, ip:%s:%d"), m_sServerIP.c_str(), m_iServerPort);
		m_pClient = new CZClientSocket(m_sServerIP.c_str(), m_iServerPort);
		_stprintf(szStatus, _T("服务器IP地址：%s，端口号：%d，%s"), m_sServerIP.c_str(), m_iServerPort,
			m_pClient->IsConnect() ? _T("连接成功") : _T("连接失败"));
		GetDlgItem(IDC_STATIC_SERVER)->SetWindowText(szStatus);
		m_pCmdQueue = new CMySimpleWStringQueue(0, 100);
	}
	else
	{
		_stprintf(szStatus, _T("服务器IP地址：%s，端口号：%d，%s"), m_sServerIP.c_str(), m_iServerPort, _T("连接失败"));
		GetDlgItem(IDC_STATIC_SERVER)->SetWindowText(szStatus);
	}


	SetTimer(TIMER_CHECK_LOG, 10000, NULL);
	SetTimer(TIMER_UPDATE_DATA, 200, NULL);
	m_pCheckThread = new CCheckDevices(this);
	m_pCheckThread->Start();
	m_pSaveThread = new CSaveImage(this);
	m_pSaveThread->Start();
	//test
	int i = 0;
	for (i = 0; i < 10; i++)
	{
		S_IMAGE_DATA *data = new S_IMAGE_DATA;
		data->iWidth = m_iImageWidth;
		data->iHeight = m_iImageHeight;
		data->iPos = 1;
		wstring strTime;
		GetCurrentTimeStr(strTime, false, true);
		data->strTime = strTime;
		data->pImg = new BYTE[m_iImageWidth*m_iImageHeight];
		memset(data->pImg, 100, m_iImageWidth*m_iImageHeight);
		AddImageToSave(data);
		Sleep(100);
	}


	if (OpenCamera())
	{
		//StartCamera();
	}
	_stprintf(szStatus, _T("硬点采集设备：%s"), IsCameraOpened() ? _T("连接成功") : _T("连接失败"));
	GetDlgItem(IDC_STATIC_CAMERA)->SetWindowText(szStatus);

	srand((unsigned)time(NULL));

	if (!m_ftpParams.sIP.empty() && m_ftpParams.iPort > 0)
	{
		m_ftpClient.SetParam(m_ftpParams);
		m_pFtpFileThread = new CFTPFile(this);
		m_pFtpFileThread->Start();
	}

	CreateUploadFiles();

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CHardPointDlg::InitBuffer()
{
	m_pImgBuffer = new S_IMAGE_DATA[m_arySize];
	int i = 0;
	for (i = 0; i < m_arySize; i++)
	{
		(*(m_pImgBuffer+i)).strTime = _T("");
		(*(m_pImgBuffer+i)).iPos = 0;
		(*(m_pImgBuffer+i)).pImg = new BYTE[m_iImageWidth*m_iImageHeight];
	}
}
void CHardPointDlg::ReleaseBuffer()
{
	int i = 0;
	for (i = 0; i < m_arySize; i++)
	{
		if (m_pImgBuffer->pImg != NULL)
		{
			delete m_pImgBuffer->pImg;
			m_pImgBuffer->pImg = NULL;
		}
	}
}
// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CHardPointDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CHardPointDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

bool CHardPointDlg::ReadConfig(void)
{
	bool bRet = false;
	if (!IsFileOrDirExists(m_szIniFile))
	{
		CAPPLog::Log(LOG_ERR, _T("[ERR]ReadConfig->config file not exists:%s"), m_szIniFile);
		MessageBox(_T("系统配置文件不存在, 请检查!"), _T("系统错误"), MB_OK);
		return bRet;
	}
	GetPrivateProfileString(_T("Commons"), _T("DataFolder"), _T(""), m_szDataPath, MAX_PATH, m_szIniFile);
	GetPrivateProfileString(_T("Commons"), _T("LineID"), _T(""), m_Device.m_wszLineID, 32, m_szIniFile);
	GetPrivateProfileString(_T("Commons"), _T("TrainID"), _T(""), m_Device.m_wszTrainID, 32, m_szIniFile);
	GetPrivateProfileString(_T("Commons"), _T("TrainBoxID"), _T(""), m_Device.m_wszTrainBoxID, 32, m_szIniFile);
	GetPrivateProfileString(_T("Commons"), _T("DeviceID"), _T(""), m_Device.m_wszDeviceID, 32, m_szIniFile);
	m_iSoftVersion = GetPrivateProfileInt(_T("Commons"), _T("SoftVersion"), 0, m_szIniFile);
	m_iConfigVersion = GetPrivateProfileInt(_T("Commons"), _T("ConfigVersion"), 0, m_szIniFile);
	m_iSoftVersionNew = GetPrivateProfileInt(_T("Commons"), _T("SoftVersionNew"), 0, m_szIniFile);
	m_iConfigVersionNew = GetPrivateProfileInt(_T("Commons"), _T("ConfigVersionNew"), 0, m_szIniFile);
	m_iChannel = GetPrivateProfileInt(_T("Commons"), _T("Channel"), 13, m_szIniFile);
	m_bSaveImg = (GetPrivateProfileInt(_T("Commons"), _T("SaveImg"), 0, m_szIniFile)>0);

	TCHAR szValue[64];
	GetPrivateProfileString(_T("Servers"), _T("ServerIP"), _T(""), szValue, MAX_PATH, m_szIniFile);
	m_sServerIP = szValue;
	m_iServerPort = GetPrivateProfileInt(_T("Servers"), _T("ServerPort"), 0, m_szIniFile);

	GetPrivateProfileString(_T("Threshold"), _T("YD"), _T("0"), szValue, 32, m_szIniFile);
	m_fYD_YZ = _ttof(szValue);
	GetPrivateProfileString(_T("Threshold"), _T("YDMin"), _T("0"), szValue, 32, m_szIniFile);
	m_f_YDMin = _ttof(szValue);
	//ImageProcess
	m_iAreaMaxWidth = GetPrivateProfileInt(_T("ImageProcess"), _T("AreaMaxWidth"), 0, m_szIniFile);
	m_iAreaMinWidth = GetPrivateProfileInt(_T("ImageProcess"), _T("AreaMinWidth"), 0, m_szIniFile);
	m_iAreaUnit = GetPrivateProfileInt(_T("ImageProcess"), _T("AreaUnit"), 0, m_szIniFile);
	m_arySize = GetPrivateProfileInt(_T("ImageProcess"), _T("ArySize"), 0, m_szIniFile);
	m_iImageWidth = GetPrivateProfileInt(_T("ImageProcess"), _T("ImageWidth"), 0, m_szIniFile);
	m_iImageHeight = GetPrivateProfileInt(_T("ImageProcess"), _T("ImageHeight"), 0, m_szIniFile);
	m_iNumThresh = GetPrivateProfileInt(_T("ImageProcess"), _T("NumThresh"), 0, m_szIniFile);
	m_iHZ = GetPrivateProfileInt(_T("ImageProcess"), _T("HZ"), 0, m_szIniFile);	
	m_iBinThresh = GetPrivateProfileInt(_T("ImageProcess"), _T("BinThresh"), 0, m_szIniFile);
	m_iRectX = GetPrivateProfileInt(_T("ImageProcess"), _T("RectX"), 0, m_szIniFile);
	m_iRectY = GetPrivateProfileInt(_T("ImageProcess"), _T("RectY"), 0, m_szIniFile);
	m_iRectWidth = GetPrivateProfileInt(_T("ImageProcess"), _T("RectWidth"), 0, m_szIniFile);
	m_iRectHeight = GetPrivateProfileInt(_T("ImageProcess"), _T("RectHeight"), 0, m_szIniFile);
	m_iLineThresh = GetPrivateProfileInt(_T("ImageProcess"), _T("LineThresh"), 0, m_szIniFile);
	m_iCountLessOrEqual = GetPrivateProfileInt(_T("ImageProcess"), _T("CountLessOrEqual"), 0, m_szIniFile);

	char szcValue[32];
	char *pszIniFile = NULL;
	int iLen = 0;
	DTCT2A(&pszIniFile, iLen, m_szIniFile);
	GetPrivateProfileStringA("FTPConfig", "FtpServerIP", "", szcValue, 32, pszIniFile);
	m_ftpParams.sIP = szcValue;
	m_ftpParams.iPort = GetPrivateProfileIntA("FTPConfig", "FtpServerPort", 21, pszIniFile);
	GetPrivateProfileStringA("FTPConfig", "FtpUser", "", szcValue, 32, pszIniFile);
	m_ftpParams.sUserName = szcValue;
	GetPrivateProfileStringA("FTPConfig", "FtpPass", "", szcValue, 32, pszIniFile);
	m_ftpParams.sPassword = szcValue;
	GetPrivateProfileStringA("FTPConfig", "FtpRootFolder", "/", m_szftpRootPath, MAX_PATH, pszIniFile);
	delete[]pszIniFile;
	pszIniFile = NULL;

	return true;
}

void CHardPointDlg::OnTimer(UINT_PTR nIDEvent)
{
	wstring sTime;
	GetCurrentTimeStr(sTime);
	if (nIDEvent == TIMER_CHECK_LOG)
	{
		//check log file
		bool bCheck = true;
		if (m_sLastDate.empty())
			m_sLastDate = sTime;
		else
		{
			bCheck = (m_sLastDate.substr(0, 8) != sTime.substr(0, 8));
			if (bCheck)
			{
				m_sLastDate = sTime;
			}
		}
		if (bCheck)
		{
			TCHAR szLogFile[MAX_PATH];
			_stprintf(szLogFile, _T("%sLog\\%s.txt"), m_szAppPath, m_sLastDate.substr(0, 8).c_str());
			if (!IsFileOrDirExists(szLogFile))
			{
				char *pszLog = NULL;
				int iPath = 0;
				DTCT2A(&pszLog, iPath, szLogFile);
				CAPPLog::SetLogFile(pszLog);
				delete[]pszLog;
				CAPPLog::Log(LOG_FORCE, _T("[FORCE]-----------a new file---------------"));
			}
		}
		//心跳
		TCHAR szInfo[32];
		_stprintf(szInfo, _T("1103%02d%02d"), m_iSoftVersion, m_iConfigVersion);
		AddIntoQueue(szInfo);

		CheckDevices();

		//检查ftp
		CheckFtp();
		CheckFtpDate();
	}
	else if (nIDEvent == TIMER_UPDATE_DATA)
	{
		return;
		TCHAR szTmp[64];

		int iUnitH = 100;
		int iPosx = 0;
		int iStart = 1000;
		int iEnd = 1250;
		iPosx = rand() % (iEnd - iStart) + iStart;
		float fYD = 1.0*iPosx*iUnitH / 1000.0;
		if (abs(m_fYD - fYD) > 0.1)
		{
			TCHAR szStatus[64];
			m_fYD = fYD;
			_stprintf(szTmp, _T("%6.1f"), m_fYD);
			GetDlgItem(IDC_EDIT_YD)->SetWindowText(szTmp);

			_stprintf(szStatus, _T("1406%6.1f"), m_fYD);
			AddIntoQueue(szStatus);

			int iStatus = -1;
			CheckFault(abs(m_fYD) > m_fYD_YZ, iStatus);
			if (iStatus >= 0)
			{
				_stprintf(szTmp, _T("硬点 %s :%6.1f(%6.1f)"), iStatus == 0 ? _T("恢复正常") : _T("故障报警"), m_fYD, m_fYD_YZ);
				CAPPLog::Log(LOG_ERR, _T("[FORCE]OnTimer->%s"), szTmp);

				if (iStatus > 0)
				{
					wstring sFault;
					GetFaultID(sFault, 7);

					_stprintf(szTmp, _T("13%s$%s$8$%6.1f"), sFault.c_str(), sTime.c_str(), m_fYD);
					AddIntoQueue(szTmp);
				}
			}
		}
	}
	CDialogEx::OnTimer(nIDEvent);
}


void CHardPointDlg::OnBnClickedBtStart()
{
	TCHAR szTmp[32];
	GetDlgItem(ID_BT_START)->GetWindowText(szTmp, 32);
	wstring sTmp = szTmp;
	if (sTmp == _T("启动"))
	{
		if (IsCameraOpened())
		{
			m_iPicOrder = 0;
			StartCamera();
		}
		else
		{
			MessageBox(_T("相机连接失败!"));
			return;
		}
		GetDlgItem(ID_BT_START)->SetWindowText(_T("停止"));
		m_bRunning = true;
	}
	else
	{
		GetDlgItem(ID_BT_START)->SetWindowText(_T("启动"));
		m_bRunning = false;
		if (IsCameraOpened())
		{
			StopCamera();
		}
	}
}

void CHardPointDlg::CheckDevices(void)
{
	TCHAR szStatus[64];
	bool bCheckResult = false;
	if (!IsCameraOpened())
	{
		OpenCamera();
	}
	//检查硬点相机连接状态
	bCheckResult = IsCameraOpened();
	if (m_bConnect && !bCheckResult || !m_bConnect && bCheckResult)
	{
		m_bConnect = bCheckResult;
		_stprintf(szStatus, _T("硬点采集设备 %s"), m_bConnect ? _T("连接成功") : _T("连接失败"));
		GetDlgItem(IDC_STATIC_CAMERA)->SetWindowText(szStatus);

		_stprintf(szStatus, _T("1207%s"), m_bConnect ? _T("00") : _T("01"));
		AddIntoQueue(szStatus);
	}

	//检查客户端网络连接
	if (m_pClient != NULL)
	{
		if (!m_pClient->IsConnect())
		{
			m_pClient->CreateSocket();
		}
		bCheckResult = m_pClient->IsConnect();
		_stprintf(szStatus, _T("服务器IP地址：%s，端口号：%d，%s"), m_sServerIP.c_str(), m_iServerPort,
			bCheckResult ? _T("连接成功") : _T("连接失败"));
		GetDlgItem(IDC_STATIC_SERVER)->SetWindowText(szStatus);
	}
}

void CHardPointDlg::SendToServer(void)
{
	if (m_pCmdQueue == NULL) return;
	if (m_pClient == NULL) return;
	if (!m_pClient->IsConnect()) return;

	wstring sData, sCmd, sOutputCmd, sOutputData;
	while (m_pCmdQueue->GetAnItem(sData) >= 0)
	{
		CAPPLog::Log(LOG_INFO, _T("[INFO]SendToServer->get a cmd:%s"), sData.c_str());
		CreateCMD(sData, _T("00"), sCmd);
		if (m_pClient->SendData(sCmd.c_str(), sCmd.length()) != sCmd.length())
		{
			CAPPLog::Log(LOG_ERR, _T("[ERR]SendToServer->failed to send cmd:%s"), sCmd.c_str());
			break;
		}
		TCHAR szBuffer[256];
		int iData = 256;
		iData = m_pClient->RecvData(szBuffer, iData);
		if (iData <= 0)
		{
			CAPPLog::Log(LOG_ERR, _T("[ERR]SendToServer->failed to recv response:%d"), iData);
			break;
		}
		sOutputCmd = szBuffer;
		wstring sEnctypeType;
		if (!ParseCMD(sOutputCmd, sEnctypeType, sOutputData))
		{
			CAPPLog::Log(LOG_ERR, _T("[ERR]SendToServer->failed to parse response:%s"), sOutputCmd.c_str());
			break;
		}
		if (sData.substr(0, 2) != sOutputData.substr(0, 2))
		{
			CAPPLog::Log(LOG_ERR, _T("[ERR]SendToServer->wrong response:%s"), sOutputCmd.c_str());
			break;
		}
		if (sOutputData.length()>=4 && _T("EE") == sOutputData.substr(2, 2))
		{
			CAPPLog::Log(LOG_ERR, _T("[ERR]SendToServer->wrong cmd word:%s"), sOutputCmd.c_str());
			break;
		}
		sCmd = sOutputData.substr(0, 2);
		if (sCmd == _T("11"))  //版本号
		{
			if (sOutputData.length() >= 16)
			{
				size_t iPos = sOutputData.find(_T("#"));
				if (iPos != wstring::npos)
				{
					wstring sTime;
					GetCurrentTimeStr(sTime);
					if (sTime != sOutputData.substr(2, 14))
					{
						CAPPLog::Log(LOG_INFO, _T("SendToServer->Set new time:%s(%s)"), sOutputData.substr(2, 14).c_str(), sTime.c_str());
						SetCurrentTimeStr(sOutputData.substr(2, iPos - 2));
					}

					sOutputData = sOutputData.substr(iPos + 1);
					int iSoftVersion = 0;
					int iConfigVersion = 0;
					TCHAR szMsg[16];
					for (int i = 0; i < sOutputData.length() / 6; i++)
					{
						iSoftVersion = _ttoi(sOutputData.substr(6 * i + 2, 2).c_str());
						iConfigVersion = _ttoi(sOutputData.substr(6 * i + 2, 2).c_str());
						if (sOutputData.substr(6 * i, 2) == _T("03"))
						{
							if (m_iSoftVersionNew < iSoftVersion)
							{
								m_iSoftVersionNew = iSoftVersion;
								_stprintf(szMsg, _T("%d"), m_iSoftVersionNew);
								WritePrivateProfileString(_T("Commons"), _T("SoftVersionNew"), szMsg, m_szIniFile);
							}
							if (m_iConfigVersionNew < iConfigVersion)
							{
								m_iConfigVersionNew = iConfigVersion;
								_stprintf(szMsg, _T("%d"), m_iConfigVersionNew);
								WritePrivateProfileString(_T("Commons"), _T("ConfigVersionNew"), szMsg, m_szIniFile);
							}
						}
					}
				}
				else
				{
					wstring sTime;
					GetCurrentTimeStr(sTime);
					if (sTime != sOutputData.substr(2, 14))
					{
						CAPPLog::Log(LOG_INFO, _T("SendToServer->Set new time:%s(%s)"), sOutputData.substr(2, 14).c_str(), sTime.c_str());
						SetCurrentTimeStr(sOutputData.substr(2));
					}
				}
			}
		}
		else if (sCmd == _T("12")) //设备故障
		{
			//返回00，不判断
		}
		else if (sCmd == _T("13")) //检测到的故障
		{
			//返回00，不判断
		}
		else if (sCmd == _T("14")) //实时数据
		{
			//无数据返回，不接收
		}
		else
		{
			CAPPLog::Log(LOG_ERR, _T("[ERR]SendToServer->wrong cmd word response:%s"), sOutputCmd.c_str());
			break;
		}

		m_pCmdQueue->RemoveItem(sData);
		CAPPLog::Log(LOG_INFO, _T("SendToServer->queue size:%d"), m_pCmdQueue->GetCount());
	}
}

bool CHardPointDlg::GrabImage(void)
{
	bool bRet = false;
	CGrabResultPtr ptrGrabResult;
	if (!IsCameraRunning()) return bRet;

	try
	{
		// Wait for an image and then retrieve it. A timeout of 5000 ms is used.
		((CInstantCamera *)m_pCamera)->RetrieveResult(1000, ptrGrabResult, TimeoutHandling_ThrowException);
		if (ptrGrabResult->GrabSucceeded())
		{
			m_iPicOrder++;
			//CAPPLog::Log(LOG_ERR, _T("\n图片计数:%d"), m_iPicOrder);
			//记录图片的获取时间，用于保存图片的名称
			wstring strTime;
			GetCurrentTimeStr(strTime, false, true);
			//
			int iWidth = ptrGrabResult->GetWidth();
			int iHeight = ptrGrabResult->GetHeight();
			CvRect rect = cvRect(0, 0, 0, 0);
			rect.x = m_iRectX;
			rect.y = m_iRectY;
			rect.width = m_iRectWidth;
			rect.height = m_iRectHeight;
			CvRect result[100];
			int iLines = 0;
			uint8_t *pImageBuffer = (uint8_t *)ptrGrabResult->GetBuffer();
			//图片逆时针旋转90度
			ImageFlip(pImageBuffer, iHeight*iWidth, iWidth, 4);

			cv::Mat *pMatImg = new cv::Mat(iWidth,iHeight,CV_8UC1, (void *)pImageBuffer);
			if (m_bSaveImg)
			{
				DWORD time1 = GetTickCount();
				TCHAR szTmp[MAX_PATH];
				//根目录+Datas+日期+车组号+车厢号
				_stprintf(szTmp, _T("%sDatas\\%s\\%s\\%s\\%02d\\%s_%d.jpg"), m_szDataPath, strTime.substr(0, 8).c_str(),
					m_Device.m_wszTrainID, m_Device.m_wszTrainBoxID, 7, strTime.c_str(), GetTickCount() % 1000);
				CAPPLog::Log(LOG_ERR, _T("第%d张图片:%s"), m_iPicOrder,szTmp);

				char *psz = NULL;
				int iLen = 0;
				DTCT2A(&psz, iLen, szTmp);
				cv::imwrite(psz, *pMatImg);
				delete[]psz;
				psz = NULL;
				DWORD time2 = GetTickCount();
				CAPPLog::Log(LOG_ERR, _T("第%d张图片保存时间:%d"), m_iPicOrder, time2-time1);
			}
			//分析图片，获取亮斑位置
			DWORD time1 = GetTickCount();
			iLines = ImageProcess(*pMatImg,iHeight, iWidth, rect, m_iBinThresh, m_iAreaMinWidth, m_iAreaMaxWidth, result, iLines);
			DWORD time2 = GetTickCount();
			CAPPLog::Log(LOG_ERR, _T("第%d张图片识别时间:%d"), m_iPicOrder, time2 - time1);

			//CAPPLog::Log(LOG_ERR, _T("iLines:%d"), iLines);

			int iLineCenter = 0;
			bool bFlag = false;//是否是硬点
			bool bReset = false;//重置数组
			bool bAppend = false;//添加到数组尾部
			//亮斑矩形中心的y坐标
			if (iLines > 0)
			{
				int j = iLines / 2;
				iLineCenter = result[j].y;
				CAPPLog::Log(LOG_ERR, "iLines:%d,亮斑中心高度:%d，当前tail：%d", iLines, iLineCenter, m_aryTail);
				if (m_aryTail > 0)
				{
					//如果当前值小于前一值
					if ((iLineCenter <= (*(m_pImgBuffer + m_aryTail - 1)).iPos))
					{
						m_iIsCalc++;
						if (m_iIsCalc > m_iCountLessOrEqual)
						{
							//判断亮点高度上升的图片数量是否大于经验阈值
							if (m_aryTail > m_iNumThresh )
							{
								bFlag = true;
							}
							else//舍弃数据
							{
								CAPPLog::Log(LOG_ERR, "图片数量%d不大于阈值%d,不进行计算，重新统计！", m_aryTail, m_iNumThresh);
								bReset = true;
							}
						}
						else//判断是否去除干扰
						{

						}
					}
					//如果当前值大于前一值，添加到数组中
					else
					{
						//m_iIsCalc = 0;
						bAppend = true;
					}
				}
				else
				{
					bAppend = true;
				}
			}
			else
			{
				TCHAR szTmp[MAX_PATH];
				//根目录+Datas+日期+车组号+车厢号
				_stprintf(szTmp, _T("%sDatas\\%s\\%s\\%s\\%02d\\%s_%d.jpg"), m_szDataPath, strTime.substr(0, 8).c_str(),
					m_Device.m_wszTrainID, m_Device.m_wszTrainBoxID, 7, strTime.c_str(), GetTickCount() % 1000);
				CAPPLog::Log(LOG_ERR, _T("第%d张图片未识别:%s"), m_iPicOrder, szTmp);

				char *psz = NULL;
				int iLen = 0;
				DTCT2A(&psz, iLen, szTmp);
				cv::imwrite(psz, *pMatImg);
				delete[]psz;
				psz = NULL;
			}

			if (bAppend)
			{
				CAPPLog::Log(LOG_ERR, "Append:iLineCenter:%d,m_aryTail:%d", iLineCenter, m_aryTail);
				//保存图片信息到数组最后
				if (m_aryTail < m_arySize)
				{
					(*(m_pImgBuffer + m_aryTail)).strTime = strTime;
					(*(m_pImgBuffer + m_aryTail)).tckTime = GetTickCount();
					(*(m_pImgBuffer + m_aryTail)).iPos = iLineCenter;
					memcpy((*(m_pImgBuffer + m_aryTail)).pImg, pImageBuffer, iWidth*iHeight);
					m_aryTail++;
				}
				else
				{
					//CAPPLog::Log(LOG_ERR, "[ERR]GrabImage->failed :buffer数组越界");
					m_aryTail = 0;
				}
				m_iIsCalc = 0;
			}

			if (bReset)
			{
				CAPPLog::Log(LOG_ERR, "Reset:m_aryTail:%d, index:%d", m_aryTail, m_iPicOrder);
				m_aryTail = 0;
				m_iIsCalc = 0;
			}

			if (bFlag)
			{
				//上升段时间
				int time = (m_pImgBuffer[m_aryTail - 1].tckTime - m_pImgBuffer[0].tckTime);//根据帧率换算成真实时间(ms)
				//上升段距离
				//int dist = m_iAreaUnit * ((*(m_pImgBuffer + m_aryTail - 1)).iPos - (*m_pImgBuffer).iPos) / 2;
				double dist = 1.0*m_iAreaUnit * ((*(m_pImgBuffer + m_aryTail-1)).iPos - (*m_pImgBuffer).iPos)/1000;
				//加速度
				double acceleration = (2 * dist / time / time)*1000;//米/平方秒
				CAPPLog::Log(LOG_ERR, "[-------计算信息--------]:持续时间:%d毫秒,距离:%f毫米,加速度:%f米/平方秒", time, dist, acceleration);

				if (acceleration > m_f_YDMin)
				{
					wstring sTime;
					GetCurrentTimeStr(sTime);
					//保存硬点信息到文件中
					WCHAR strFilePathName[MAX_PATH];
					_stprintf(strFilePathName, _T("%s%02d\\"), m_szDataPath, m_iChannel);
					TestandCreateDirectory(strFilePathName);
					_stprintf(strFilePathName, _T("%s%02d\\Hrd%s.txt"), m_szDataPath, m_iChannel, sTime.substr(0,8).c_str());
					fstream file(strFilePathName, ios::app);

					char chBuf[100];
					//sprintf(chBuf, "硬点发生时间：%s，持续时间：%02u毫秒，距离:%02u毫米，加速度：%6.2f米/平方秒", strTime, time, dist, acceleration);
					//时间,硬点持续时间,硬点持续距离,硬点数值
					sprintf(chBuf, "%s,%02u,%02u,%6.2f", strTime, time, dist, acceleration);
					string str(chBuf);
					file << str;
					file << std::endl;
					file.close();
					//UI界面显示
					//发送数据
					TCHAR szStatus[64];
					_stprintf(szStatus, _T("%6.1f"), acceleration);
					GetDlgItem(IDC_EDIT_YD)->SetWindowText(szStatus);

					
					m_fYD = acceleration;
					_stprintf(szStatus, _T("1406%6.1f"), m_fYD);
					AddIntoQueue(szStatus);

					int iStatus = -1;
					CheckFault(abs(m_fYD) > m_fYD_YZ, iStatus);
					if (iStatus >= 0)
					{
						WCHAR szTmp[100];
						_stprintf(szTmp, _T("硬点 %s :%6.1f(%6.1f)"), iStatus == 0 ? _T("恢复正常") : _T("故障报警"), m_fYD, m_fYD_YZ);
						CAPPLog::Log(LOG_ERR, _T("[FORCE]OnTimer->%s"), szTmp);

						if (iStatus > 0)
						{
							wstring sFault;
							GetFaultID(sFault, m_iChannel);

							_stprintf(szTmp, _T("13%s$%s$8$%6.1f"), sFault.c_str(), sTime.c_str(), m_fYD);
							AddIntoQueue(szTmp);
						}
					}
					//保存硬点图片
					int i = 0;
					for (i = 0; i < m_aryTail - 2;i++)
					{
						S_IMAGE_DATA data;
						data.iWidth = m_iImageWidth;
						data.iHeight = m_iImageHeight;
						data.iPos = (*(m_pImgBuffer + i)).iPos;
						data.strTime = (*(m_pImgBuffer + i)).strTime;
						data.pImg = new BYTE[m_iImageWidth*m_iImageHeight];
						memcpy(data.pImg, (*(m_pImgBuffer + i)).pImg, m_iImageWidth*m_iImageHeight);
						AddImageToSave(&data);
					}
				}
				m_aryTail = 0;
			}
		}
		bRet = true;
	}
	catch (const GenericException &e)
	{
		CAPPLog::Log(LOG_ERR, "[ERR]GrabImage->failed :%s", e.GetDescription());
		if (m_pCamera != NULL)
		{
			PylonTerminate();
			delete m_pCamera;
			m_pCamera = NULL;
		}
	}
	return bRet;
}

void CHardPointDlg::AddIntoQueue(const wstring &sData)
{
	if (!m_sServerIP.empty() && (m_iServerPort>0) && m_pCmdQueue!=NULL)
	{
		m_pCmdQueue->PutAnItem(sData);
	}
}

void CHardPointDlg::CheckFault(bool bFault, int &iStatus)
{
	iStatus = -1;
	if (bFault)
	{
		if (m_arrCheckTimes[0] == 0)
		{
			m_arrCheckTimes[1]++;
			m_arrCheckTimes[2] = 0;
			if (m_arrCheckTimes[1] > 3)
			{
				m_arrCheckTimes[0] = 1;
				iStatus = 1;
			}
		}
	}
	else
	{
		if (m_arrCheckTimes[0] == 1)
		{
			m_arrCheckTimes[2]++;
			m_arrCheckTimes[1] = 0;
			if (m_arrCheckTimes[2] > 3)
			{
				m_arrCheckTimes[0] = 0;
				iStatus = 0;
			}
		}
	}
}

void CHardPointDlg::GetFaultID(wstring &sFault, int iChannel)
{
	sFault.clear();
	TCHAR szFault[32];
	wstring sTime;
	GetCurrentTimeStr(sTime);
	_stprintf(szFault, _T("%s%s%02d%s"), m_Device.m_wszTrainID, m_Device.m_wszTrainBoxID, iChannel, sTime.c_str());
	sFault = szFault;
}

bool CHardPointDlg::OpenCamera()
{
	bool bRet = false;

	CloseCamera();

	PylonInitialize();
	 
	try
	{
		// Create an instant camera object with the camera device found first.
		m_pCamera=new CInstantCamera(CTlFactory::GetInstance().CreateFirstDevice());    

		// Print the model name of the camera.
		//cout << "Using device " << camera.GetDeviceInfo().GetModelName() << endl;

		// The parameter MaxNumBuffer can be used to control the count of buffers
		// allocated for grabbing. The default value of this parameter is 10.
		((CInstantCamera *)m_pCamera)->MaxNumBuffer = 5;

		((CInstantCamera *)m_pCamera)->Open();
		INodeMap& nodemap = ((CInstantCamera *)m_pCamera)->GetNodeMap();
		CEnumerationPtr TriggerMode(nodemap.GetNode("TriggerMode"));
		if (IsWritable(TriggerMode))
		{
			TriggerMode->FromString("On");
		}
		else
		{
			CAPPLog::Log(LOG_ERR, "TriggerMode失败，IsWritable(TriggerMode)不可写");
		}
		// Start the grabbing of c_countOfImagesToGrab images.
		// The camera device is parameterized with a default configuration which
		// sets up free-running continuous acquisition.
		//((CInstantCamera *)m_pCamera)->StartGrabbing();
		if (m_pGrabThread == NULL)
		{
			m_pGrabThread = new CGrabImage(this);
		}

		// This smart pointer will receive the grab result data.
		bRet = true;
	}
	catch (const GenericException &e)
	{
		CAPPLog::Log(LOG_ERR, "[ERR]OpenCamera->failed :%s", e.GetDescription());
	}

	return bRet;
}

bool CHardPointDlg::CloseCamera()
{
	if (m_pGrabThread != NULL)
	{
		if (m_pGrabThread->IsRuning()) m_pGrabThread->Stop();
		delete m_pGrabThread;
		m_pGrabThread = NULL;
	}
	if (m_pCamera != NULL)
	{
		StopCamera();
		((CInstantCamera *)m_pCamera)->Close();
		PylonTerminate();
		delete m_pCamera;
		m_pCamera = NULL;
	}
	return true;
}

bool CHardPointDlg::StartCamera()
{
	if (!IsCameraOpened()) return false;

	if (!IsCameraRunning())
	{
		((CInstantCamera *)m_pCamera)->StartGrabbing();
	}
	if (IsCameraRunning())
	{
		TCHAR szTmp[MAX_PATH];
		wstring sTime;
		GetCurrentTimeStr(sTime);
		//根目录+Datas+日期+车组号+车厢号
		_stprintf(szTmp, _T("%sDatas\\%s\\%s\\%s\\%02d\\"), m_szDataPath, sTime.substr(0, 8).c_str(),
			m_Device.m_wszTrainID, m_Device.m_wszTrainBoxID, 7);
		TestandCreateDirectory(szTmp);

		if (!m_pGrabThread->IsRuning()) m_pGrabThread->Start();
	}

	return IsCameraRunning();
}

bool CHardPointDlg::StopCamera()
{
	if (!IsCameraOpened()) return false;

	if (IsCameraRunning())
	{
		if (m_pGrabThread != NULL)
		{
			if (m_pGrabThread->IsRuning()) m_pGrabThread->Stop();
		}
		((CInstantCamera *)m_pCamera)->StopGrabbing();
	}
	return !IsCameraRunning();
}

bool CHardPointDlg::IsCameraOpened()
{
	return (m_pCamera != NULL && ((CInstantCamera *)m_pCamera)->IsPylonDeviceAttached());
}

bool CHardPointDlg::IsCameraRunning()
{
	bool bRet = false;
	if (m_pCamera != NULL)
	{
		bRet = ((CInstantCamera *)m_pCamera)->IsGrabbing();
	}
	return bRet;
}

void CHardPointDlg::AddImageToSave(S_IMAGE_DATA *pImg)
{
	m_pSaveThread->AddImage(pImg);
}

void CHardPointDlg::GetDataPath(TCHAR * pStr)
{
	wstring sTime;
	GetCurrentTimeStr(sTime);
	//根目录+Datas+日期+车组号+车厢号
	_stprintf(pStr, _T("%sDatas\\%s\\%s\\%s\\%d"), m_szDataPath, sTime.substr(0, 8).c_str(),
		m_Device.m_wszTrainID, m_Device.m_wszTrainBoxID, m_iChannel);
	TestandCreateDirectory(pStr);
}
int CHardPointDlg::GetArySize()
{
	return m_arySize;
}
void CHardPointDlg::FtpFile(void)
{
	if (m_pFtpFileQueue == NULL) return;
	if (!m_ftpClient.IsConnect()) return;
	string sFile;
	if (m_pFtpFileQueue->GetAnItem(sFile) >= 0)
	{
		string sDir = sFile;
		size_t iPos = sDir.rfind("\\");
		if (iPos == string::npos)
		{
			m_pFtpFileQueue->RemoveItem(sFile);
			CAPPLog::Log(LOG_ERR, "FtpFile->wrong file:%s", sFile.c_str());
			return;
		}
		sDir.erase(iPos + 1);
		char *psz = NULL;
		int iLen = 0;
		DTCT2A(&psz, iLen, m_szDataPath);
		iPos = sDir.find(psz);
		if (iPos == string::npos)
		{
			m_pFtpFileQueue->RemoveItem(sFile);
			CAPPLog::Log(LOG_ERR, "FtpFile->wrong file:%s, data root:%s", sFile.c_str(), psz);
			delete[]psz;
			return;
		}
		delete[]psz;

		sDir = sDir.substr(iPos + _tcslen(m_szDataPath));
		replace(sDir.begin(), sDir.end(), '\\', '/');
		sDir = m_szftpRootPath + sDir;
		if (m_ftpClient.UploadFile(sFile, sDir))
		{
			m_pFtpFileQueue->RemoveItem(sFile);
			CAPPLog::Log(LOG_ERR, "FtpFile->ok to upload:%s", sFile.c_str());
		}
		else
		{
			CAPPLog::Log(LOG_ERR, "FtpFile->failed to upload:%s", sFile.c_str());
		}
	}
}

void CHardPointDlg::CheckFtp(void)
{
	static int s_status = -1;
	if (m_ftpParams.sIP.empty() || m_ftpParams.iPort <= 0) return;
	if (!m_ftpClient.IsConnect())
	{
		m_ftpClient.LoginFtp();
	}
	bool bRefresh = false;
	int iStatus = m_ftpClient.IsConnect() ? 1 : 0;
	if (s_status == -1)
	{
		s_status = iStatus;
		bRefresh = true;
	}
	else if (s_status != iStatus)
	{
		s_status = iStatus;
		bRefresh = true;
	}
	if (bRefresh)
	{
		TCHAR szMsg[64];
		_stprintf(szMsg, _T("ftp服务器 %s"), s_status == 1 ? _T("连接成功") : _T("连接失败"));
		GetDlgItem(IDC_STATIC_FTP)->SetWindowText(szMsg);
	}
}

void CHardPointDlg::CreateUploadFiles(void)
{
	if (m_sftpDate != _T("00000000")) return;
	TCHAR szDate[16];
	wstring sCurTime;
	wstring sDate;
	GetCurrentTimeStr(sDate);
	sDate = sDate.substr(0, 8) + _T("120000");
	TimePlusOrMinus(sDate, -24 * 3600, sCurTime);
	GetPrivateProfileString(_T("FTPConfig"), _T("FinishedDate"), _T(""), szDate, MAX_PATH, m_szIniFile);
	sDate = szDate;
	if (sDate.empty())
	{
		sDate = sCurTime;
	}
	vector<string> vect;
	wstring sAddDate;
	while (sDate.substr(0, 8) < sCurTime.substr(0, 8))
	{
		TimePlusOrMinus(sDate, 24 * 3600, sAddDate);

		CAPPLog::Log(LOG_INFO, _T("CreateUploadFiles->search date:%s"), sAddDate.substr(0, 8).c_str());
		TCHAR szPath[MAX_PATH];
		//根目录+Datas+日期+车组号+车厢号
		_stprintf(szPath, _T("%sDatas\\%s\\"), m_szDataPath, sAddDate.substr(0, 8).c_str());
		char *pszPath = NULL;
		int iLen = 0;
		DTCT2A(&pszPath, iLen, szPath);
		vect.clear();
		EnumFilesInFolder(pszPath, "txt", vect);
		delete[]pszPath;
		pszPath = NULL;
		for (int i = 0; i < vect.size(); i++)
		{
			m_pFtpFileQueue->PutAnItem(vect[i]);
		}
		sDate = sAddDate;
	}
	m_sftpDate = sDate;
}

void CHardPointDlg::CheckFtpDate(void)
{
	if (m_sftpDate == _T("00000000") || m_sftpDate.empty()) return;

	if (m_pFtpFileQueue->GetCount() == 0)
	{
		WritePrivateProfileString(_T("FTPConfig"), _T("FinishedDate"), m_sftpDate.c_str(), m_szIniFile);
		m_sftpDate.clear();
	}
}


