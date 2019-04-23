
// HardPointDlg.h : 头文件
//
#include "HXZYSocket.h"
#include "cestask.h"
#include "MyTestQueue.h"
#include "CommonData.h"
#include <pylon/PylonIncludes.h>
#include "ftp/FTPClient.h"

using namespace Pylon;

#pragma once

using namespace std;

struct S_IMAGE_DATA
{
	//图片采集时间(精确到毫秒)
	wstring strTime;
	//计算后的高度信息（单位：微米）
	int iPos;
	//图片数据
	BYTE* pImg;
	//图片宽度
	int iWidth;
	//图片高度
	int iHeight;
	//
	DWORD tckTime;

	S_IMAGE_DATA()
	{
		strTime = _T("");
		iPos = 0;
		pImg = NULL;
		iWidth = 0;
		iHeight = 0;
		tckTime = 0;
	}
	S_IMAGE_DATA(const S_IMAGE_DATA & data)
	{
		strTime = data.strTime;
		iPos = data.iPos;
		pImg = data.pImg;
		iWidth = data.iWidth;
		iHeight = data.iHeight;
		tckTime = data.tckTime;
	}

	S_IMAGE_DATA &operator=(const S_IMAGE_DATA & data)
	{
		strTime = data.strTime;
		iPos = data.iPos;
		pImg = data.pImg;
		iWidth = data.iWidth;
		iHeight = data.iHeight;
		tckTime = data.tckTime;
		return *this;
	}
	BOOL operator==(const S_IMAGE_DATA & data)
	{
		return (
			strTime == data.strTime
			//iPos == data.iPos &&
			//pImg == data.pImg &&
			//iWidth == data.iWidth &&
			//iHeight == data.iHeight &&
			//tckTime == data.tckTime
			);
	}
};

class CHardPointDlg;
class CCheckDevices : public CESTask
{
public:
	CCheckDevices(CHardPointDlg *pDlg);
	~CCheckDevices();

protected:
	virtual DWORD Process();
private:
	CHardPointDlg *m_pDlg;
};
//保存硬点图片线程
class CSaveImage : public CESTask
{
public:
	CSaveImage(CHardPointDlg *pDlg);
	~CSaveImage();
	void AddImage(S_IMAGE_DATA *pImg);
protected:
	virtual DWORD Process();
private:
	vector<S_IMAGE_DATA> *m_pImg;
	CMySimpleQueue<S_IMAGE_DATA> *m_pImgQueue;
	CHardPointDlg *m_pDlg;
};
class CGrabImage : public CESTask
{
public:
	CGrabImage(CHardPointDlg *pDlg);
	~CGrabImage();

protected:
	virtual DWORD Process();
private:
	CHardPointDlg *m_pDlg;
};

class CFTPFile : public CESTask
{
public:
	CFTPFile(CHardPointDlg *pDlg);
	~CFTPFile();

protected:
	virtual DWORD Process();
private:
	CHardPointDlg *m_pDlg;
};

// CHardPointDlg 对话框
class CHardPointDlg : public CDialogEx
{
// 构造
public:
	CHardPointDlg(CWnd* pParent = NULL);	// 标准构造函数
	~CHardPointDlg();	// 析构函数

	void CheckDevices(void);
	void SendToServer(void);
	bool GrabImage(void);
	void FtpFile(void);

// 对话框数据
	enum { IDD = IDD_HARDPOINT_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持

	void InitBuffer();
	void ReleaseBuffer();

// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
private:
	TCHAR m_szAppPath[MAX_PATH];
	TCHAR m_szIniFile[MAX_PATH];
	TCHAR m_szDataPath[MAX_PATH];
	wstring m_sLastDate;
	//是否保存图片
	bool m_bSaveImg;
	//通道号
	int m_iChannel;
	S_DEVICE m_Device;
	//软件版本号，如果版本号为1.0则此处为10
	int m_iSoftVersion;
	//配置版本号，如果版本号为1.0则此处为10
	int m_iConfigVersion;
	//软件新版本号，如果版本号为1.0则此处为10
	int m_iSoftVersionNew;
	//配置新版本号，如果版本号为1.0则此处为10
	int m_iConfigVersionNew;

	bool m_bRunning;
	wstring m_sServerIP;
	int m_iServerPort;
	bool m_bConnect;
	CZClientSocket *m_pClient;

	CMySimpleWStringQueue *m_pCmdQueue;
	CCheckDevices *m_pCheckThread;
	CGrabImage *m_pGrabThread;
	CSaveImage *m_pSaveThread;
	//记录上一硬点
	float m_fYD;
	//大于此阈值时报警
	float m_fYD_YZ;
	//大于此值时认为是硬点
	float m_f_YDMin;
	//[0]-状态 0-正常 1-故障
	//[1]-错误计数
	//[2]-正确计数
	int m_arrCheckTimes[3];

	bool ReadConfig(void);
	void AddIntoQueue(const wstring &sData);
	void CheckFault(bool bFault, int &iStatus);
	void GetFaultID(wstring &sFault, int iChannel);


	void *m_pCamera;
	bool OpenCamera();
	bool CloseCamera();
	bool StartCamera();
	bool StopCamera();
	bool IsCameraOpened();
	bool IsCameraRunning();

	int m_iPicOrder;
	int m_iIsCalc;
	//图片信息
	int m_arySize;//读取配置文件
	S_IMAGE_DATA *m_pImgBuffer;
	int m_aryTail;
	//连续几次小于或等于前值时，认为弓运动方向反转
	int m_iCountLessOrEqual;
	//连续多少个高度信息相等时，舍弃数据，重新计算
	int m_iEqual;
	//亮点的最大宽度
	int m_iAreaMaxWidth;
	//亮点的最小宽度
	int m_iAreaMinWidth;
	//面阵图像单像素对应的长度，单位微米
	int m_iAreaUnit;
	//图片信息，给m_pImageData分配内存用
	//图片宽度
	int m_iImageWidth;
	//图片高度
	int m_iImageHeight;
	//连续多少张图片中的连点连续上升认定为硬点
	int m_iNumThresh;
	//相机采集频率
	int m_iHZ;

	//图像二值化阈值
	int m_iBinThresh;
	//检测区域左上角X
	int m_iRectX;
	//检测区域左上角Y
	int m_iRectY;
	//检测区域宽度
	int m_iRectWidth;
	//检测区域高度
	int m_iRectHeight;
	//至少连续多少行满足条件认为是亮点
	int m_iLineThresh;

	//ftp上传
	ClsFtpClient m_ftpClient;
	char m_szftpRootPath[MAX_PATH];
	FTP_CLIENT_PARAM m_ftpParams;
	CMySimpleStringQueue *m_pFtpFileQueue;
	CFTPFile *m_pFtpFileThread;
	wstring m_sftpDate;
	void CheckFtp(void);
	void CheckFtpDate(void);
	void CreateUploadFiles(void);

public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedBtStart();
	void AddImageToSave(S_IMAGE_DATA *pImg);
	void GetDataPath(TCHAR *);
	int GetArySize();
};
