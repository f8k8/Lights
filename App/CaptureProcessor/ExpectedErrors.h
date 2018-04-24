#pragma once

// Below are lists of errors expect from Dxgi API calls when a transition event like mode change, PnpStop, PnpStart
// desktop switch, TDR or session disconnect/reconnect. In all these cases we want the application to clean up the threads that process
// the desktop updates and attempt to recreate them.
// If we get an error that is not on the appropriate list then we exit the application

// These are the errors we expect from general Dxgi API due to a transition
extern HRESULT SystemTransitionsExpectedErrors[];

// These are the errors we expect from IDXGIOutput1::DuplicateOutput due to a transition
extern HRESULT CreateDuplicationExpectedErrors[];

// These are the errors we expect from IDXGIOutputDuplication methods due to a transition
extern HRESULT FrameInfoExpectedErrors[];

// These are the errors we expect from IDXGIAdapter::EnumOutputs methods due to outputs becoming stale during a transition
extern HRESULT EnumOutputsExpectedErrors[];

extern void SetAppropriateEvent(HRESULT result, HRESULT expectedErrors[], HANDLE expectedErrorEvent, HANDLE unexpectedErrorEvent);