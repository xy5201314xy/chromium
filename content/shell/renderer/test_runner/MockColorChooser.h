// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockColorChooser_h
#define MockColorChooser_h

#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebNonCopyable.h"
#include "third_party/WebKit/public/web/WebColorChooser.h"
#include "third_party/WebKit/public/web/WebColorChooserClient.h"

namespace WebTestRunner {

class WebTestDelegate;
class WebTestProxyBase;
class MockColorChooser : public blink::WebColorChooser, public blink::WebNonCopyable {
public:
    MockColorChooser(blink::WebColorChooserClient*, WebTestDelegate*, WebTestProxyBase*);
    virtual ~MockColorChooser();

    // blink::WebColorChooser implementation.
    virtual void setSelectedColor(const blink::WebColor) OVERRIDE;
    virtual void endChooser() OVERRIDE;

    void invokeDidEndChooser();
    WebTaskList* taskList() { return &m_taskList; }
private:
    blink::WebColorChooserClient* m_client;
    WebTestDelegate* m_delegate;
    WebTestProxyBase* m_proxy;
    WebTaskList m_taskList;
};

}

#endif // MockColorChooser_h
