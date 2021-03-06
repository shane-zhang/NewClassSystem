#include "StdAfx.h"
#include<iostream>
#include<cstring>
#include<dshow.h>
#include<windows.h>
#include<conio.h>
#pragma comment(lib, "strmiids")
#pragma comment(lib, "quartz")

bool Bstr_Compare(BSTR,BSTR);//Function to compare BSTR strings
void HR_Failed(HRESULT hr);// hr status function
IMoniker* Device_Read(ICreateDevEnum*,IMoniker*,GUID,BSTR);//Device reading function
IBaseFilter* Device_Init(IMoniker*,IBaseFilter*);//Function to initialize Input/Output devices
void Device_Addition(IGraphBuilder*,IBaseFilter*,BSTR);//Function to add device to graph

using namespace std;

HRESULT AddGraphToRot(IUnknown *pUnkGraph, DWORD *pdwRegister)
{
	IMoniker * pMoniker;
	IRunningObjectTable *pROT;
	WCHAR wsz[128];
	HRESULT hr;
	if (!pUnkGraph || !pdwRegister)
		return E_POINTER;
	if (FAILED(GetRunningObjectTable(0, &pROT)))
		return E_FAIL;

    hr = StringCchPrintfW(wsz, NUMELMS(wsz), L"FilterGraph %08x pid %08x\0", (DWORD_PTR)pUnkGraph,
              GetCurrentProcessId());

    hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr))
    {
        // Use the ROTFLAGS_REGISTRATIONKEEPSALIVE to ensure a strong reference
        // to the object.  Using this flag will cause the object to remain
        // registered until it is explicitly revoked with the Revoke() method.
        //
        // Not using this flag means that if GraphEdit remotely connects
        // to this graph and then GraphEdit exits, this object registration
        // will be deleted, causing future attempts by GraphEdit to fail until
        // this application is restarted or until the graph is registered again.
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph,
                            pMoniker, pdwRegister);
        pMoniker->Release();
    }

    pROT->Release();
    return hr;
}

void RemoveGraphFromRot(DWORD pdwRegister)
{
    IRunningObjectTable *pROT;

    if (SUCCEEDED(GetRunningObjectTable(0, &pROT)))
    {
        pROT->Revoke(pdwRegister);
        pROT->Release();
    }
}

int main (void)
{
	HRESULT hr;	// COM result
	IGraphBuilder *pGraph = NULL;// Main graphbuilder pointer
	IMediaControl *pControl = NULL;	// Media Control interface
	ICreateDevEnum *pDeviceEnum = NULL;// System device enumerator
	IBaseFilter *pInputDevice = NULL, *pOutputDevice = NULL, *pImDevice = NULL;// Input and output filters(devices)
	IMoniker *pDeviceMonik = NULL;// Device moniker
	GUID DEVICE_CLSID ;// GUID i.e.  CLSID_Xxxxxxxxxxx
	BSTR bstrDeviceName= {'\0'};

	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED );// Initialise COM
	if (FAILED(hr))
	{
		HR_Failed(hr);
		return hr;
	}
	hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,IID_IGraphBuilder, (void**)&pGraph);//Initialize Graph builder
	if (FAILED(hr))
	{
		HR_Failed(hr);
		return hr;
	}
	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,IID_ICreateDevEnum, (void **)&pDeviceEnum);//Initialize Device enumerator
	if (FAILED(hr))
	{
		HR_Failed(hr);
		return hr;
	}
	hr = pGraph->QueryInterface(IID_IMediaControl,(void**)&pControl);// Query interface for IMediaControl
	if (FAILED(hr))
	{
		HR_Failed(hr);
		return hr;
	}
/**********************************************************************************/
//Init the devices
//Default Capture Device
	DEVICE_CLSID = AM_KSCATEGORY_CAPTURE;// the input device category
	bstrDeviceName = SysAllocString(L"USB Audio Device");// device name as seen in Graphedit.exe
	pDeviceMonik = Device_Read(pDeviceEnum,pDeviceMonik,DEVICE_CLSID,bstrDeviceName);//read the required device 
	pInputDevice = Device_Init(pDeviceMonik,pInputDevice);//Return the device after initializing it
	Device_Addition(pGraph,pInputDevice,bstrDeviceName);//add device to graph
	SysFreeString(bstrDeviceName);
//Default Transform Device
	DEVICE_CLSID = CLSID_LegacyAmFilterCategory;// the audio renderer device category
	bstrDeviceName = SysAllocString(L"ACM Wrapper");// device name as seen in Graphedit.exe
	pDeviceMonik = Device_Read(pDeviceEnum,pDeviceMonik,DEVICE_CLSID,bstrDeviceName);//read the required device
	pImDevice = Device_Init(pDeviceMonik,pImDevice);//Return the device after initializing it
	Device_Addition(pGraph,pImDevice,bstrDeviceName);//add device to graph
	SysFreeString(bstrDeviceName);
//Default output device
	DEVICE_CLSID = AM_KSCATEGORY_CAPTURE;// the audio renderer device category
	bstrDeviceName = SysAllocString(L"USB Audio Device");// device name as seen in Graphedit.exe
	pDeviceMonik = Device_Read(pDeviceEnum,pDeviceMonik,DEVICE_CLSID,bstrDeviceName);//read the required device
	pOutputDevice = Device_Init(pDeviceMonik,pOutputDevice);//Return the device after initializing it
	Device_Addition(pGraph,pOutputDevice,bstrDeviceName);//add device to graph
	SysFreeString(bstrDeviceName);
//Get the information about the Capture device and media
	IPin *pInPin = NULL, *pOutPin = NULL, *pWrapper1 = NULL, *pWrapper2 = NULL;
	pInputDevice->FindPin(L"1",&pInPin);
	pImDevice->FindPin(L"In",&pWrapper1);
	pImDevice->FindPin(L"Out",&pWrapper2);
	pOutputDevice->FindPin(L"2",&pOutPin);
	pGraph->ConnectDirect(pInPin,pWrapper1,NULL);
	hr = pGraph->ConnectDirect(pWrapper2,pOutPin,NULL);

	HR_Failed(hr);

	DWORD dwGraphRegister;
	AddGraphToRot(pControl, &dwGraphRegister);
	pControl->Run();
	cout<<"Close the window to exit or hit any key"<<endl;
	 while(!_kbhit())
	 {
	 }
	pControl->Release();//Release control
	pDeviceEnum->Release();//Release Device enumerator
	pGraph->Release();//Release the Graph
}


bool Bstr_Compare(BSTR bstrFilter,BSTR bstrDevice)
{
	bool flag = true;
	int strlenFilter = SysStringLen(bstrFilter);//set string length
	int strlenDevice = SysStringLen(bstrDevice);//set string length
	char* chrFilter = (char*)malloc(strlenFilter+1);// allocate memory
	char* chrDevice = (char*)malloc(strlenDevice+1);// allocate memory
	int j = 0;

	if (strlenFilter!=strlenDevice)//if the strings are of not the same length,means they totall different strings
		flag = false;//sety flag to false to indicate "not-same" strings
	else
	{
		for(; j < strlenFilter;j++)//now, copy 1 by 1 each char to chrFilter and chrDevice respectively
		{
			chrFilter[j] = (char)bstrFilter[j];//copy
			chrDevice[j] = (char)bstrDevice[j];//copy
			cout<<j;
			
		}
		chrFilter[strlenFilter] = '\0';//add terminating character
		chrDevice[strlenDevice] = '\0';//add terminating character

		for(j=0; j < strlenFilter;j++)//check loop
		{
			if(chrFilter[j] != chrDevice[j])//check if there are chars that are not samne
				flag = false;//if chars are not same, set flag to false to indicate "not-same" strings
		}
		
		if(flag == true && j == strlenFilter-1)//see if we went through the 'check loop' 
			flag = true;//means strings are same
	}
return flag;
}
void HR_Failed(HRESULT hr)
{
	TCHAR szErr[MAX_ERROR_TEXT_LEN];
        DWORD res = AMGetErrorText(hr, szErr, MAX_ERROR_TEXT_LEN);
        if (res == 0)
        {
            StringCchPrintf(szErr, MAX_ERROR_TEXT_LEN, L"Unknown Error: 0x%2x", hr);
        }
    
		MessageBox(0, szErr, TEXT("Error!"), MB_OK | MB_ICONERROR);
		return;
}


IMoniker* Device_Read(ICreateDevEnum* pDeviceEnum,IMoniker *pDeviceMonik,GUID DEVICE_CLSID,BSTR bstrDeviceName)
{
	HRESULT hr;
	IEnumMoniker *pEnumCat = NULL;// Device enumeration moniker
	VARIANT varName;

	hr = pDeviceEnum->CreateClassEnumerator(DEVICE_CLSID, &pEnumCat, 0);// Enumerate the specified device, distinguished by DEVICE_CLSID

	if (hr == S_OK) 
	{
    ULONG cFetched;
	while (pEnumCat->Next(1, &pDeviceMonik, &cFetched) == S_OK)//Pickup as moniker
	{
        IPropertyBag *pPropBag = NULL;
        hr = pDeviceMonik->BindToStorage(0, 0, IID_IPropertyBag,(void **)&pPropBag);//bind the properties of the moniker
        if (SUCCEEDED(hr))
        {
			VariantInit(&varName);// Initialise the variant data type
			hr = pPropBag->Read(L"FriendlyName", &varName, 0);
      		if (SUCCEEDED(hr))
            {	
				if(Bstr_Compare(varName.bstrVal,bstrDeviceName) == true)//make a comparison
				{	
					wcout<<varName.bstrVal<<" found"<<endl;
					return pDeviceMonik;
				}
			}
			else HR_Failed(hr);
			VariantClear(&varName);//clear the variant data type
			pPropBag->Release();//release the properties
        }
		else HR_Failed(hr);
		pDeviceMonik->Release();//release Device moniker
	}
	pEnumCat->Release();//release category enumerator
	}
	else HR_Failed(hr);
	return NULL;
}

IBaseFilter* Device_Init(IMoniker* pDeviceMonik,IBaseFilter* pDevice)
{
	HRESULT hr;
	hr = pDeviceMonik->BindToObject(NULL, NULL, IID_IBaseFilter,(void**)&pDevice);//Instantiate the device
	if (SUCCEEDED(hr))
	{
		cout<<"Device initiation successful..."<<endl;
	}
	else HR_Failed(hr);

return pDevice;
}

void Device_Addition(IGraphBuilder* pGraph,IBaseFilter* pDevice,BSTR bstrName)
{
	HRESULT hr;
	hr = pGraph->AddFilter(pDevice,bstrName);
	if(SUCCEEDED(hr))
		{
			wcout<<"Addition of "<<bstrName<<" successful..."<<endl;
		}
		else HR_Failed(hr);
}