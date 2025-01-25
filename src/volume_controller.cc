#pragma once
#define _WINSOCKAPI_
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <stdio.h>
#include <iostream>
#include <nan.h>
#include <wrl/client.h> 

using Microsoft::WRL::ComPtr;

template <typename... Args>
std::string string_format(std::string format, Args... args)
{
  size_t size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
  std::unique_ptr<char[]> buf(new char[size]);
  std::snprintf(buf.get(), size, format.c_str(), args...);
  return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

void checkErrors(HRESULT hr, std::string error_message)
{
  if (FAILED(hr))
  {
    throw string_format("%s (0x%X)", error_message.c_str(), hr);
  }
}

class VolumeControl
{
private:
  ComPtr<IAudioEndpointVolume> device;

public:
  VolumeControl()
  {
    // The IMMDeviceEnumerator interface provides methods for enumerating multimedia device resources. Basically the interface of the requested object.
    ComPtr<IMMDeviceEnumerator> deviceEnumerator;

    // Creates a single uninitialized object of the class associated with a specified CLSID
    checkErrors(
      CoCreateInstance(
        __uuidof(MMDeviceEnumerator),   // Requested COM device enumerator id
        NULL,                           // If NULL, indicates that the object is not being created as part of an aggregate.
        CLSCTX_INPROC_SERVER,           // Context in which the code that manages the newly created object will run (Same process).
        IID_PPV_ARGS(&deviceEnumerator) // Address of pointer variable that receives the interface pointer requested in riid.
      ),
      "Error when trying to get a handle to MMDeviceEnumerator device enumerator");

    // Device interface pointer where we will dig the audio device endpoint
    ComPtr<IMMDevice> defaultDevice;

    checkErrors(
      deviceEnumerator->GetDefaultAudioEndpoint(
        eRender,       // Audio rendering stream. Audio data flows from the application to the audio endpoint device, which renders the stream. eCapture would be the opposite
        eConsole,      // The role that the system has assigned to an audio endpoint device. eConsole for games, system notification sounds, and voice commands
        &defaultDevice // Pointer to default audio enpoint device
      ),
      "Error when trying to get a handle to the default audio enpoint");

    checkErrors(
      defaultDevice->Activate(            // Creates a COM object with the specified interface.
        __uuidof(IAudioEndpointVolume), // Reference to a GUID that identifies the interface that the caller requests be activated
        CLSCTX_INPROC_SERVER,           // Context in which the code that manages the newly created object will run (Same process).
        NULL,                           // Set NULL to activate the IAudioEndpointVolume endpoint https://msdn.microsoft.com/en-us/library/ms679029.aspx
        &device                         //  Pointer to a pointer variable into which the method writes the address of the interface specified by parameter iid. Through this method, the caller obtains a counted reference to the interface.
      ),
      "Error when trying to get a handle to the volume endpoint");
  }

  BOOL isMuted()
  {
    BOOL muted = false;

    checkErrors(device->GetMute(&muted), "getting muted state");

    return muted;
  }

  void setMuted(BOOL muted)
  {
    checkErrors(device->SetMute(muted, NULL), "setting mute");
  }

  float getVolume()
  {
    float currentVolume = 0;

    checkErrors(
      device->GetMasterVolumeLevelScalar(&currentVolume),
      "getting volume");

    return currentVolume;
  }

  void setVolume(float volume)
  {
    if (volume < 0.0 || volume > 1.0)
    {
      throw std::string("Volume needs to be between 0.0 and 1.0 inclusive");
    }

    checkErrors(
      device->SetMasterVolumeLevelScalar(volume, NULL),
      "setting volume");
  }
};

class VolumeControlWrapper : public Nan::ObjectWrap
{
public:
  static NAN_MODULE_INIT(Init)
  {
    auto tpl = Nan::New<v8::FunctionTemplate>(New);
    tpl->SetClassName(Nan::New("VolumeControl").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(tpl, "getVolume", GetVolume);
    Nan::SetPrototypeMethod(tpl, "setVolume", SetVolume);
    Nan::SetPrototypeMethod(tpl, "isMuted", IsMuted);
    Nan::SetPrototypeMethod(tpl, "setMuted", SetMuted);
    Nan::SetPrototypeMethod(tpl, "execTranslatorMacro", ExecTranslatorMacro);

    constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
    Nan::Set(target, Nan::New("VolumeControl").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
  }

private:
  VolumeControl device;

  static NAN_METHOD(New)
  {
    if (info.IsConstructCall())
    {
      std::cout << "Constructing new object" << std::endl;
      try
      {
        auto obj = new VolumeControlWrapper();
        obj->Wrap(info.This());
        info.GetReturnValue().Set(info.This());
        std::cout << "Constructed new object" << std::endl;
      }
      catch (std::string e)
      {
        return Nan::ThrowError(Nan::New(e).ToLocalChecked());
      }
    }
    else
    {
      return Nan::ThrowError(Nan::New("The constructor cannot be called as a function.").ToLocalChecked());
    }
  }

  static NAN_METHOD(GetVolume)
  {
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try
    {
      info.GetReturnValue().Set(obj->device.getVolume());
    }
    catch (std::string e)
    {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }

  static NAN_METHOD(SetVolume)
  {
    if (info.Length() != 1)
    {
      return Nan::ThrowError(Nan::New("Exactly one number parameter is required.").ToLocalChecked());
    }

    double volume = Nan::To<double>(info[0]).ToChecked();
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try
    {
      obj->device.setVolume(volume);
    }
    catch (std::string e)
    {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }

  static NAN_METHOD(IsMuted)
  {
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try
    {
      info.GetReturnValue().Set(obj->device.isMuted());
    }
    catch (std::string e)
    {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }

  static NAN_METHOD(SetMuted)
  {
    if (info.Length() != 1)
    {
      return Nan::ThrowError(Nan::New("Exactly one boolean parameter is required.").ToLocalChecked());
    }

    bool muted = Nan::To<bool>(info[0]).ToChecked();
    auto obj = Nan::ObjectWrap::Unwrap<VolumeControlWrapper>(info.Holder());
    try
    {
      obj->device.setMuted(muted);
    }
    catch (std::string e)
    {
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }

  static NAN_METHOD(ExecTranslatorMacro)
  {
    if (info.Length() != 1)
    {
      return Nan::ThrowError(Nan::New("Exactly one string parameter is required.").ToLocalChecked());
    }

    const int CopyDataID = 24;
    std::string widowName = "Translator CopyData Target";

    HWND hwnd = FindWindowA(NULL, widowName.c_str());
    if (hwnd == 0)
    {
      return Nan::ThrowError(Nan::New("Could not find running Translator instance to send message to").ToLocalChecked());
    }

    v8::Local<v8::String> v8MacroName = Nan::To<v8::String>(info[0]).ToLocalChecked();
    std::string wstrMacroName = "Macro: ";
    wstrMacroName.append(*Nan::Utf8String(v8MacroName));

    LPCSTR lpszMacroName = wstrMacroName.c_str();
    COPYDATASTRUCT cds;
    cds.dwData = CopyDataID;
    cds.cbData = strlen(lpszMacroName);
    cds.lpData = (PVOID)lpszMacroName;

    SetLastError(ERROR_SUCCESS);
    PDWORD_PTR lpdwResult = 0;
    LRESULT ret = SendMessageTimeoutA(hwnd, WM_COPYDATA, (WPARAM)0, (LPARAM)(LPVOID)&cds, SMTO_ABORTIFHUNG, 5000, lpdwResult);

    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    if (ret == 0 && FAILED(hr))
    {
      std::string e = string_format("%s (0x%X)", "Failed to execute Translator Macro", hr);
      return Nan::ThrowError(Nan::New(e).ToLocalChecked());
    }
  }

  static inline Nan::Persistent<v8::Function> &constructor()
  {
    static Nan::Persistent<v8::Function> constructorFunction;
    return constructorFunction;
  }
};

void UnInitialize(void *)
{
  CoUninitialize();
}

NAN_MODULE_INIT(InitModule)
{
  CoInitialize(NULL);

  VolumeControlWrapper::Init(target);

  node::AddEnvironmentCleanupHook(Nan::GetCurrentContext()->GetIsolate(), UnInitialize, (void *)NULL);
}

NODE_MODULE(addon, InitModule)
