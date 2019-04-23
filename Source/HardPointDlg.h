
// HardPointDlg.h : ͷ�ļ�
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
	//ͼƬ�ɼ�ʱ��(��ȷ������)
	wstring strTime;
	//�����ĸ߶���Ϣ����λ��΢�ף�
	int iPos;
	//ͼƬ����
	BYTE* pImg;
	//ͼƬ���
	int iWidth;
	//ͼƬ�߶�
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
//����Ӳ��ͼƬ�߳�
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

// CHardPointDlg �Ի���
class CHardPointDlg : public CDialogEx
{
// ����
public:
	CHardPointDlg(CWnd* pParent = NULL);	// ��׼���캯��
	~CHardPointDlg();	// ��������

	void CheckDevices(void);
	void SendToServer(void);
	bool GrabImage(void);
	void FtpFile(void);

// �Ի�������
	enum { IDD = IDD_HARDPOINT_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��

	void InitBuffer();
	void ReleaseBuffer();

// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
private:
	TCHAR m_szAppPath[MAX_PATH];
	TCHAR m_szIniFile[MAX_PATH];
	TCHAR m_szDataPath[MAX_PATH];
	wstring m_sLastDate;
	//�Ƿ񱣴�ͼƬ
	bool m_bSaveImg;
	//ͨ����
	int m_iChannel;
	S_DEVICE m_Device;
	//����汾�ţ�����汾��Ϊ1.0��˴�Ϊ10
	int m_iSoftVersion;
	//���ð汾�ţ�����汾��Ϊ1.0��˴�Ϊ10
	int m_iConfigVersion;
	//����°汾�ţ�����汾��Ϊ1.0��˴�Ϊ10
	int m_iSoftVersionNew;
	//�����°汾�ţ�����汾��Ϊ1.0��˴�Ϊ10
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
	//��¼��һӲ��
	float m_fYD;
	//���ڴ���ֵʱ����
	float m_fYD_YZ;
	//���ڴ�ֵʱ��Ϊ��Ӳ��
	float m_f_YDMin;
	//[0]-״̬ 0-���� 1-����
	//[1]-�������
	//[2]-��ȷ����
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
	//ͼƬ��Ϣ
	int m_arySize;//��ȡ�����ļ�
	S_IMAGE_DATA *m_pImgBuffer;
	int m_aryTail;
	//��������С�ڻ����ǰֵʱ����Ϊ���˶�����ת
	int m_iCountLessOrEqual;
	//�������ٸ��߶���Ϣ���ʱ���������ݣ����¼���
	int m_iEqual;
	//����������
	int m_iAreaMaxWidth;
	//�������С���
	int m_iAreaMinWidth;
	//����ͼ�����ض�Ӧ�ĳ��ȣ���λ΢��
	int m_iAreaUnit;
	//ͼƬ��Ϣ����m_pImageData�����ڴ���
	//ͼƬ���
	int m_iImageWidth;
	//ͼƬ�߶�
	int m_iImageHeight;
	//����������ͼƬ�е��������������϶�ΪӲ��
	int m_iNumThresh;
	//����ɼ�Ƶ��
	int m_iHZ;

	//ͼ���ֵ����ֵ
	int m_iBinThresh;
	//����������Ͻ�X
	int m_iRectX;
	//����������Ͻ�Y
	int m_iRectY;
	//���������
	int m_iRectWidth;
	//�������߶�
	int m_iRectHeight;
	//������������������������Ϊ������
	int m_iLineThresh;

	//ftp�ϴ�
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
