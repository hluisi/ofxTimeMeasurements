/*
 *  ofxTimeMeasurements.cpp
 *  emptyExample
 *
 *  Created by Oriol Ferrer Mesià on 28/01/11.
 *  Copyright 2011 uri.cat. All rights reserved.
 *
 */

#include "ofxTimeMeasurements.h"
#include <float.h>


ofxTimeMeasurements* ofxTimeMeasurements::singleton = NULL;

ofxTimeMeasurements::ofxTimeMeasurements(){

	currentFrameNum = 0;
	uiScale = 1.0;
	desiredFrameRate = 60.0f; //assume 60
	enabled = true;
	timeAveragePercent = 0.02;
	averaging = false;
	msPrecision = 1;
	maxW = 27;
	drawAuto = true;
	internalBenchmark = false;

	#if defined(USE_OFX_FONTSTASH)
	useFontStash = false;
	#endif

	#if defined(USE_OFX_HISTORYPLOT)
	plotHeight = 60;
	numAllocatdPlots = 0;
	plotBaseY = 0;
	plotResolution = 1;
	maxPlotSamples = 4096;
	numActivePlots = 0;
	#endif

	mainThreadID = getThreadID();

	bgColor = ofColor(0);
	hilightColor = ofColor(44,77,255) * 1.5;
	disabledTextColor = ofColor(255,0,128);
	measuringColor = ofColor(0,130,0);
	frozenColor = hilightColor * 1.5;

	dimColorA = 40;

	freeze = false;

	idleTimeColorFadePercent = 0.5;
	idleTimeColorDecay = 0.96;
	deadThreadExtendedLifeDecSpeed = 0.975;

	longestLabel = 0;
	selection = TIME_MEASUREMENTS_UPDATE_KEY;
	drawLocation = TIME_MEASUREMENTS_BOTTOM_RIGHT;
	numVisible = 0;

	enableKey = TIME_MEASUREMENTS_GLOBAL_TOGGLE_KEY;
	activateKey = TIME_MEASUREMENTS_INTERACT_KEY;
	toggleSampleKey = TIME_MEASUREMENTS_TOGGLE_SAMPLE_KEY;

	menuActive = false;

	int numHues = 9;
	float brightness = 190.0f;
	for (int i = 0; i < numHues; i++) {
		float hue = fmod( i * (255.0f / float(numHues)), 255.0f);
		ofColor c = ofColor::fromHsb(hue, 255.0f, brightness, 255.0f);
		threadColorTable.push_back(c);
	}

	numThreads = 0;
	configsDir = ".";
	removeExpiredThreads = true;
	settingsLoaded = false;

	charW = 8; //ofBitmap font char w
	charH = TIME_MEASUREMENTS_LINE_HEIGHT;

	//used for internal benchmark ('B')
	wastedTimeThisFrame = wastedTimeAvg = 0;
	wastedTimeDrawingThisFrame = wastedTimeDrawingAvg = 0;

#if (OF_VERSION_MINOR >= 8)
		//-100 and +100 are to make sure we are always the first AND last at update and draw events, so we can sum everyone's times
		#if (OF_VERSION_MINOR < 9)
			ofAddListener(ofEvents().setup, this, &ofxTimeMeasurements::_beforeSetup, OF_EVENT_ORDER_BEFORE_APP - 100);
			ofAddListener(ofEvents().setup, this, &ofxTimeMeasurements::_afterSetup, OF_EVENT_ORDER_AFTER_APP + 100);
		#endif
		ofAddListener(ofEvents().update, this, &ofxTimeMeasurements::_beforeUpdate, OF_EVENT_ORDER_BEFORE_APP - 100);
		ofAddListener(ofEvents().update, this, &ofxTimeMeasurements::_afterUpdate, OF_EVENT_ORDER_AFTER_APP + 100);
		ofAddListener(ofEvents().draw, this, &ofxTimeMeasurements::_beforeDraw, OF_EVENT_ORDER_BEFORE_APP - 100);
		ofAddListener(ofEvents().draw, this, &ofxTimeMeasurements::_afterDraw, OF_EVENT_ORDER_AFTER_APP + 100);
		ofAddListener(ofEvents().keyPressed, this, &ofxTimeMeasurements::_beforeKeyPressed, OF_EVENT_ORDER_BEFORE_APP - 100);
		ofAddListener(ofEvents().keyPressed, this, &ofxTimeMeasurements::_afterKeyPressed, OF_EVENT_ORDER_AFTER_APP + 100);
//		ofAddListener(ofEvents().keyReleased, this, &ofxTimeMeasurements::_beforeKeyReleased, OF_EVENT_ORDER_BEFORE_APP - 100);
//		ofAddListener(ofEvents().keyReleased, this, &ofxTimeMeasurements::_afterKeyReleased, OF_EVENT_ORDER_AFTER_APP + 100);

		ofAddListener(ofEvents().keyPressed, this, &ofxTimeMeasurements::_keyPressed, OF_EVENT_ORDER_BEFORE_APP - 200);
		ofAddListener(ofEvents().exit, this, &ofxTimeMeasurements::_appExited); //to save to xml
		#if defined(USE_OFX_HISTORYPLOT)
		ofAddListener(ofEvents().windowResized, this, &ofxTimeMeasurements::_windowResized); //to save to xml
		#endif
#else
	#if (OF_VERSION == 7 && OF_VERSION_MINOR >= 2 )
		ofAddListener(ofEvents().update, this, &ofxTimeMeasurements::_beforeUpdate);
		ofAddListener(ofEvents().update, this, &ofxTimeMeasurements::_afterUpdate);
		ofAddListener(ofEvents().draw, this, &ofxTimeMeasurements::_afterDraw);
		ofAddListener(ofEvents().draw, this, &ofxTimeMeasurements::_beforeDraw);
	#else
		ofAddListener(ofEvents.update, this, &ofxTimeMeasurements::_afterUpdate);
		ofAddListener(ofEvents.update, this, &ofxTimeMeasurements::_beforeUpdate);
		ofAddListener(ofEvents.draw, this, &ofxTimeMeasurements::_afterDraw);
		ofAddListener(ofEvents.draw, this, &ofxTimeMeasurements::_beforeDraw);
	#endif
#endif
}


void ofxTimeMeasurements::addSetupHooks(){
	#if (OF_VERSION_MINOR >= 9)
	ofAddListener(ofEvents().setup, this, &ofxTimeMeasurements::_beforeSetup, OF_EVENT_ORDER_BEFORE_APP - 100);
	ofAddListener(ofEvents().setup, this, &ofxTimeMeasurements::_afterSetup, OF_EVENT_ORDER_AFTER_APP + 100);
	#endif
}


void ofxTimeMeasurements::_windowResized(ofResizeEventArgs &e){
	#if defined(USE_OFX_HISTORYPLOT)
	int hist = plotResolution * e.width;
	map<string, ofxHistoryPlot*>::iterator it = plots.begin();
	while(it != plots.end()){
		if(it->second != NULL){
			it->second->setMaxHistory(MIN(maxPlotSamples, hist));
		}
		++it;
	}
	#endif
}


void ofxTimeMeasurements::setThreadColors(const vector<ofColor> & tc){
	threadColorTable.clear();
	threadColorTable = tc;
}


ofxTimeMeasurements* ofxTimeMeasurements::instance(){
	if (!singleton){   // Only allow one instance of class to be generated.
		singleton = new ofxTimeMeasurements();
	}
	return singleton;
}


void ofxTimeMeasurements::setConfigsDir(string d){
	configsDir = d;
	loadSettings(); //as we load settings on construction time, lets try re-load settings
					//a second time when a new configs location is given
}


void ofxTimeMeasurements::setDeadThreadTimeDecay(float decay){
	deadThreadExtendedLifeDecSpeed = ofClamp(decay, idleTimeColorDecay, 1.0);
}


float ofxTimeMeasurements::getLastDurationFor(const string & ID){

	float r = 0.0f;
	unordered_map<string,TimeMeasurement*>::iterator it;
	it = times.find(ID);
	if ( it != times.end() ){	//not found!
		r = times[ID]->duration / 1000.0f; //to ms
	}
	return r;
}


float ofxTimeMeasurements::getAvgDurationFor(const string & ID){

	float r = 0.0f;
	unordered_map<string,TimeMeasurement*>::iterator it;
	it = times.find(ID);
	if ( it != times.end() ){	//not found!
		r = times[ID]->avgDuration / 1000.0f; //to ms
	}
	return r;
}

void ofxTimeMeasurements::setHighlightColor(ofColor c){
	hilightColor = c;
	threadInfo[mainThreadID].color = c;
}


bool ofxTimeMeasurements::startMeasuring(const string & ID, bool accumulate, bool ifClause){

	if (!enabled) return true;

	uint64_t wastedTime;
	if(internalBenchmark){
		wastedTime = TM_GET_MICROS();
	}

	if (!settingsLoaded){
		loadSettings();
		settingsLoaded = true;
	}

	string threadName = "Thread";
	ThreadId thread = getThreadID();
	bool bIsMainThread = isMainThread(thread);

	if(!bIsMainThread){
		if(Poco::Thread::current()){
			threadName = Poco::Thread::current()->getName();
		}
	}

	mutex.lock();

	unordered_map<ThreadId, ThreadInfo>::iterator threadIt = threadInfo.find(thread);
	ThreadInfo * tinfo = NULL;
	core::tree<string> *tr = NULL;

	bool newThread = threadIt == threadInfo.end();

	if (newThread){ //new thread!

		//cout << "NewThread! " << ID << " " << &thread << endl;
		threadInfo[thread] = ThreadInfo();
		tinfo = &threadInfo[thread];
		tr = &tinfo->tree; //easier to read, tr is our tree from now on

		string tName = bIsMainThread ? "Main Thread" : (threadName + "(" + ofToString(numThreads) + ")");
		//init the iterator
		*tr = tName; //thread name is root
		tinfo->tit = (core::tree<string>::iterator)*tr;
		tinfo->order = numThreads;

		if (!bIsMainThread){
			tinfo->color = threadColorTable[numThreads%(threadColorTable.size())];
			numThreads++;
		}else{ //main thread
			tinfo->color = hilightColor;
		}

	}else{
		tinfo = &threadInfo[thread];
		tr = &(tinfo->tree); //easier to read, tr is our tree from now on
	}

	//see if we had an actual measurement, or its a new one
	unordered_map<string, TimeMeasurement*>::iterator tit = times.find(ID);
	TimeMeasurement* t;

	if(tit == times.end()){ //if it wasnt in the tree, append it
		tinfo->tit = tinfo->tit.push_back(ID);
	}else{
		core::tree<string>::iterator temptit = tr->tree_find_depth(ID);
		if(temptit != tr->end()){
			tinfo->tit = temptit;
		}else{
			//cout << "gotcha!" << endl;
			//this is the rare case where we already had a measurement for this ID,
			//but it must be assigned to another old thread bc we cant find it!
			//so we re-add that ID for this thread and update the tree iterator
			tinfo->tit = tinfo->tit.push_back(ID);
		}
	}

	if (tit == times.end()){ //not found, let alloc a new TimeMeasurement
		times[ID] = t = new TimeMeasurement();
		unordered_map<string, TimeMeasurementSettings>::iterator it2 = settings.find(ID);
		if (it2 != settings.end()){
			times[ID]->settings = settings[ID];
		}
	}else{
		t = tit->second;
	}

	t->key = ID;
	t->life = 1.0f; //
	t->measuring = true;
	t->ifClause = ifClause;
	t->microsecondsStop = 0;
	t->accumulating = accumulate;
	if(accumulate) t->numAccumulations++;
	t->error = false;
	t->frame = ofGetFrameNum();
	t->updatedLastFrame = true;
	t->microsecondsStart = TM_GET_MICROS();
	t->thread = thread;

	mutex.unlock();

	if(internalBenchmark){
		wastedTimeThisFrame += t->microsecondsStart - wastedTime;
	}

	return t->settings.enabled;
}


float ofxTimeMeasurements::stopMeasuring(const string & ID, bool accumulate){

	if (!enabled) return 0.0f;
	float ret = 0.0f;

	uint64_t timeNow = TM_GET_MICROS(); //get the time before the lock() to avoid affecting

	ThreadId thread = getThreadID();
	bool bIsMainThread = isMainThread(thread);

	mutex.lock();

	unordered_map<ThreadId, ThreadInfo>::iterator threadIt = threadInfo.find(thread);

	ThreadInfo & tinfo = threadInfo[thread];
	core::tree<string> & tr = tinfo.tree; //easier to read, tr is our tree from now on
	core::tree<string>::iterator & tit = tinfo.tit;
	if (tit.out() != tr.end()){
		tit = tit.out();
	}else{
		ofLogError("ofxTimeMeasurements") << "tree climbing too high up! (" << ID << ")";
	}

	unordered_map<string,TimeMeasurement*>::iterator it;
	it = times.find(ID);

	if ( it == times.end() ){	//not found!
		ofLog( OF_LOG_WARNING, "ID (%s)not found at stopMeasuring(). Make sure you called"
			  " startMeasuring with that ID first.", ID.c_str());
	}else{

		TimeMeasurement* t = it->second;
		if ( t->measuring ){
			t->measuring = false;
			t->thread = thread;
			t->error = false;
			t->acrossFrames = (bIsMainThread && t->frame != ofGetFrameNum()); //we only care about across-frames in main thread
			t->microsecondsStop = timeNow;
			ret = t->duration = t->microsecondsStop - t->microsecondsStart;
			if (!freeze) {
				if (!averaging) {
					t->avgDuration = t->duration;
				}
				else {
					t->avgDuration = (1.0f - timeAveragePercent) * t->avgDuration + t->duration * timeAveragePercent;
				}
			}
			if (accumulate && !freeze){
				t->microsecondsAccum += t->avgDuration;
			}
		}else{	//wrong use, start first, then stop
			t->error = true;
			ofLog( OF_LOG_WARNING, "Can't stopMeasuring(%s). Make sure you called startMeasuring"
				  " with that ID first.", ID.c_str());
		}
	}

	mutex.unlock();

	if(internalBenchmark){
		wastedTimeThisFrame += TM_GET_MICROS() - timeNow;
	}

	return ret / 1000.0f; //convert to ms
}


void ofxTimeMeasurements::setDrawLocation(ofxTMDrawLocation l, ofVec2f p){
	drawLocation = l;
	customDrawLocation = p;
}


void ofxTimeMeasurements::_afterDraw(ofEventArgs &d){
	stopMeasuring(TIME_MEASUREMENTS_DRAW_KEY, false);
	if(drawAuto){
		autoDraw();
	}
};


void ofxTimeMeasurements::autoDraw(){

	float yy = 0;
	#ifdef USE_OFX_HISTORYPLOT
	yy = numActivePlots * plotHeight;
	#endif

	switch(drawLocation){

		case TIME_MEASUREMENTS_TOP_LEFT:
			draw(TIME_MEASUREMENTS_EDGE_GAP_H,TIME_MEASUREMENTS_EDGE_GAP_V);
			break;
		case TIME_MEASUREMENTS_TOP_RIGHT:
			draw( ofGetWidth() / uiScale - getWidth() - TIME_MEASUREMENTS_EDGE_GAP_H,
				 TIME_MEASUREMENTS_EDGE_GAP_V);
			break;
		case TIME_MEASUREMENTS_BOTTOM_LEFT:
			draw(TIME_MEASUREMENTS_EDGE_GAP_H,
				 ofGetHeight() / uiScale - getHeight() - TIME_MEASUREMENTS_EDGE_GAP_V - yy);
			break;
		case TIME_MEASUREMENTS_BOTTOM_RIGHT:
			draw( ofGetWidth() / uiScale - getWidth() - TIME_MEASUREMENTS_EDGE_GAP_H,
				 ofGetHeight() / uiScale - getHeight() - TIME_MEASUREMENTS_EDGE_GAP_V - yy);
			break;
		case TIME_MEASUREMENTS_CUSTOM_LOCATION:
			draw(customDrawLocation.x, customDrawLocation.y);
			break;
	}
}


void ofxTimeMeasurements::updateLongestLabel(){

	longestLabel = 0;
	for( int i = 0; i < drawLines.size(); i++ ){

		TimeMeasurement *t = drawLines[i].tm;
		if (t){
			if (t->settings.visible){ //kinda redundant ...
				int len = drawLines[i].formattedKey.length();
				if (len > longestLabel){
					longestLabel = len;
				}
			}
		}
	}
}


void ofxTimeMeasurements::draw(int x, int y) {

	if (!enabled) return;

	uint64_t timeNow;
	if(internalBenchmark){
		timeNow = TM_GET_MICROS();
	}
	currentFrameNum = ofGetFrameNum();

	drawLines.clear();
	float percentTotal = 0.0f;
	float timePerFrame = 1000.0f / desiredFrameRate;

	mutex.lock();

	vector<TimeMeasurement*> toResetUpdatedLastFrameFlag;

	//update time stuff, build draw lists
	for( unordered_map<string,TimeMeasurement*>::iterator ii = times.begin(); ii != times.end(); ++ii ){
		TimeMeasurement* t = ii->second;
		string key = ii->first;
		if(!t->measuring){
			if (t->life > 0.01){
				t->life *= idleTimeColorDecay; //decrease life
			}else{ //life decays very slow when very low
				t->life *= deadThreadExtendedLifeDecSpeed; //decrease life very slowly
			}
		}
//		if (!t->updatedLastFrame && averaging){ // if we didnt update that time, make it tend to zero slowly
//			t->avgDuration = (1.0f - timeAveragePercent) * t->avgDuration;
//		}
		toResetUpdatedLastFrameFlag.push_back(t);
	}

	unordered_map<ThreadId, ThreadInfo>::iterator ii;
	vector<ThreadId> expiredThreads;

	//lets make sure the Main Thread is always on top
	vector< ThreadContainer > sortedThreadList;

	for( ii = threadInfo.begin(); ii != threadInfo.end(); ++ii ){ //walk all thread trees
		ThreadContainer cont;
		cont.id = ii->first;
		cont.info = &ii->second;
		if (isMainThread(ii->first)){ //is main thread
			sortedThreadList.insert(sortedThreadList.begin(), cont);
		}else{
			sortedThreadList.push_back(cont);
		}
	}
	std::sort(sortedThreadList.begin(), sortedThreadList.end(), compareThreadPairs);

	#if defined(USE_OFX_HISTORYPLOT)
	vector<ofxHistoryPlot*> plotsToDraw;
	#endif

	for( int k = 0; k < sortedThreadList.size(); k++ ){ //walk all thread trees

		ThreadId thread = sortedThreadList[k].id;
		core::tree<string> &tr = sortedThreadList[k].info->tree;

		ThreadInfo & tinfo = threadInfo[thread];
		PrintedLine header;
		header.formattedKey = "+ " + *tr;
		header.color = tinfo.color;
		header.lineBgColor = ofColor(header.color, dimColorA * 2); //header twice as alpha
		header.key = *tr; //key for selection, is thread name
		drawLines.push_back(header); //add header to drawLines

		int numAlive = 0;
		int numAdded = 0;

		core::tree<string>::iterator wholeTreeWalker = tr.in();
		bool finishedWalking = false;
		float winW = ofGetWidth();

		while( !finishedWalking ){

			string key = *wholeTreeWalker;
			TimeMeasurement * t = times[*wholeTreeWalker];
			if(t->thread == thread){

				#if defined(USE_OFX_HISTORYPLOT)
				bool plotActive = false;
				ofxHistoryPlot* plot = plots[key];
				if(plot){
					if(t->settings.plotting){
						if(t->updatedLastFrame){
							//update plot res every now and then
							if(currentFrameNum%120 == 1) plot->setMaxHistory(MIN(maxPlotSamples, winW * plotResolution));
							if (!freeze) {
								if (t->accumulating) {
									plot->update(t->microsecondsAccum / 1000.0f);
								}
								else {
									plot->update(t->avgDuration / 1000.0f);
								}
							}
						}
						plotsToDraw.push_back(plot);
						plotActive = true;
					}
				}
				#endif

				bool visible = t->settings.visible;
				bool alive = t->life > 0.0001;
				if(alive){
					numAlive++;
				}

				if (visible){
					PrintedLine l;
					l.key = key;
					l.tm = t;
					l.lineBgColor = ofColor(tinfo.color, dimColorA);

					int depth = wholeTreeWalker.level();
					for(int i = 0; i < depth; ++i) l.formattedKey += " ";

					if (wholeTreeWalker.size() == 0){
						l.formattedKey += "-";
					}else{
						l.formattedKey += "+";
					}
					l.formattedKey += key + string(t->accumulating ? "[" + ofToString(t->numAccumulations)+ "]" : "" );
					l.isAccum = t->accumulating;
					l.time = getTimeStringForTM(t);

					l.color = tinfo.color * ((1.0 - idleTimeColorFadePercent) + idleTimeColorFadePercent * t->life);
					if (!t->settings.enabled){
						if(t->ifClause){
							l.color = disabledTextColor;
						}else{
							l.color = disabledTextColor.getInverted();
						}
					}

					#if defined(USE_OFX_HISTORYPLOT)
					if(plotActive){
						l.plotColor = ofColor(plots[key]->getColor(), 200);
					}
					#endif

					if (menuActive && t->key == selection){
						if(currentFrameNum%5 < 4){
							l.color.invert();
							l.lineBgColor = ofColor(tinfo.color, dimColorA * 1.5);
						}
					}

					drawLines.push_back(l);
					numAdded++;
				}

				//only update() and draw() count to the final %
				if(key == TIME_MEASUREMENTS_DRAW_KEY || key == TIME_MEASUREMENTS_UPDATE_KEY){
					percentTotal += (t->avgDuration * 0.1f) / timePerFrame;
				}
				//reset accumulator
				t->accumulating = false;
				t->numAccumulations = 0;
				t->microsecondsAccum = 0;
			}

			//control the iterator to walk the tree "recursively" without doing so.
			if(wholeTreeWalker.size()){
				wholeTreeWalker = wholeTreeWalker.in();
			}else{
				if ( wholeTreeWalker.next() == wholeTreeWalker.end() ){
					wholeTreeWalker = wholeTreeWalker.out();
					while( wholeTreeWalker.next() == wholeTreeWalker.end() && wholeTreeWalker != tr){
						wholeTreeWalker = wholeTreeWalker.out();
					}
					if(wholeTreeWalker == tr){
						finishedWalking = true;
					}else{
						wholeTreeWalker++;
					}
				}else{
					++wholeTreeWalker;
				}
			}
		}

		#if defined(USE_OFX_HISTORYPLOT)
		numActivePlots = plotsToDraw.size();
		#endif

		if (numAlive == 0 && removeExpiredThreads){
			//drop that whole section if all entries in it are not alive
			for(int i = 0; i < numAdded + 1; i++){
				if(drawLines.size() > 0){
					int delID = drawLines.size() - 1;
					//clear selection if needed
					if (selection == drawLines[delID].key){
						selection = TIME_MEASUREMENTS_UPDATE_KEY;
					}
					drawLines.erase(drawLines.begin() + delID);
				}
			}
			expiredThreads.push_back(thread);
		}
	}

	//delete expired threads
	for(int i = 0; i < expiredThreads.size(); i++){
		unordered_map<ThreadId, ThreadInfo>::iterator treeIt = threadInfo.find(expiredThreads[i]);
		if (treeIt != threadInfo.end()) threadInfo.erase(treeIt);
	}

	mutex.unlock();

	updateLongestLabel();

	//find headers
	int tempMaxW = -1;
	vector<int> headerLocations;
	for( int i = 0; i < drawLines.size(); i++ ){
		if (drawLines[i].tm){ //its a measurement
			//add padding to draw in columns
			for(int j = drawLines[i].formattedKey.length(); j < longestLabel; j++){
				drawLines[i].formattedKey += " ";
			}
			if (!drawLines[i].tm->error){
				drawLines[i].fullLine = drawLines[i].formattedKey + " " + drawLines[i].time;
			}else{
				drawLines[i].fullLine = drawLines[i].formattedKey + "    Error!" ;
			}
			int len = drawLines[i].fullLine.length();
			if(len > tempMaxW) tempMaxW = len;
		}else{ //its a header
			drawLines[i].fullLine = drawLines[i].formattedKey;
			headerLocations.push_back(i);
		}
	}

	int numInstructionLines = 0;
	if(menuActive){ //add instructions line if menu active
		PrintedLine l;
		//title line
		l.color = hilightColor;
		l.lineBgColor = ofColor(hilightColor, dimColorA * 2);
		l.fullLine = " KEYBOARD COMMANDS "; //len = 23
		int numPad = 2 + ceil((getWidth() - charW * (23)) / charW);
		for(int i = 0; i < floor(numPad/2.0); i++ ) l.fullLine = "#" + l.fullLine;
		for(int i = 0; i < ceil(numPad/2.0); i++ ) l.fullLine += "#";
		l.fullLine = " " + l.fullLine;
		drawLines.push_back(l); numInstructionLines++;
		//key command lines
		l.lineBgColor = ofColor(hilightColor, dimColorA);
		l.fullLine = " 'UP/DOWN' select measur."; drawLines.push_back(l); numInstructionLines++;
		l.fullLine = " 'LFT/RGHT' expand/collaps"; drawLines.push_back(l); numInstructionLines++;
		l.fullLine = " 'RET' toggle code section"; drawLines.push_back(l); numInstructionLines++;
		l.fullLine = " 'A' average measurements"; drawLines.push_back(l); numInstructionLines++;
		l.fullLine = " 'F' freeze measurements"; drawLines.push_back(l); numInstructionLines++;
		l.fullLine = " 'L' cycle widget location"; drawLines.push_back(l); numInstructionLines++;
		l.fullLine = " 'PG_DWN' en/disable addon"; drawLines.push_back(l); numInstructionLines++;
		l.fullLine = " 'B' internal benchmark"; drawLines.push_back(l); numInstructionLines++;
		#if defined USE_OFX_HISTORYPLOT
		l.fullLine = " 'P' plot selectd measur."; drawLines.push_back(l); numInstructionLines++;
		#endif
	}

	maxW = tempMaxW;

	ofSetupScreen(); //mmmm----
	ofPushStyle();
	ofSetRectMode(OF_RECTMODE_CORNER);
	ofSetDrawBitmapMode(OF_BITMAPMODE_SIMPLE);
	ofEnableAlphaBlending();

	ofPushMatrix();
	ofScale(uiScale,uiScale);

	ofFill();

	//draw all plots
	#if defined(USE_OFX_HISTORYPLOT)
	//int numCols = plotsToDraw.size()
	for(int i = 0; i < plotsToDraw.size(); i++){
		int y = (plotBaseY == 0 ? ofGetHeight() : plotBaseY) / uiScale - plotHeight * (i + 1);
		plotsToDraw[i]->draw(0, y, ofGetWidth() / uiScale, plotHeight);
		ofSetColor(99);
		if(i != plotsToDraw.size() -1){
			ofLine(0, y, ofGetWidth() / uiScale, y );
		}
	}
	#endif

	float totalW = getWidth();
	float totalH = getHeight();

	//draw bg rect
	ofSetColor(bgColor);
	ofRect(x, y + 1, totalW, totalH);

	//draw all lines
	for(int i = 0; i < drawLines.size(); i++){
		ofSetColor(drawLines[i].lineBgColor);
		ofRect(x, y + 2 + i * charH, totalW, charH + (drawLines[i].tm ? 0 : 1));
		if(drawLines[i].isAccum && drawLines[i].tm != NULL){
			ofSetColor(drawLines[i].color, 128);
			ofRect(x + totalW, y + 4 + i * charH, -5, charH - 2 );
		}
		ofSetColor(drawLines[i].color);
		drawString(drawLines[i].fullLine, x , y + (i + 1) * charH);
		if(drawLines[i].plotColor.a > 0){ //plot highlight on the sides
			ofSetColor(drawLines[i].plotColor);
			float y1 = y + 2.4f + i * charH;
			ofDrawTriangle(	x, y1, 
							x, y1 + charH, 
							x + charW * 0.7f, y1 + charH * 0.5f);
		}
	}

	if(internalBenchmark){
		float offset = 0;
		if(drawLocation == TIME_MEASUREMENTS_TOP_LEFT ||
		   drawLocation == TIME_MEASUREMENTS_TOP_RIGHT ||
		   drawLocation == TIME_MEASUREMENTS_CUSTOM_LOCATION ){
			offset = (drawLines.size() + 2.5) * charH;
		}
		ofSetColor(0);
		ofRect(x, offset + y - charH, totalW, charH);
		ofSetColor(currentFrameNum%3 ? 255 : 64);
		drawString(" Meas: " + ofToString(wastedTimeAvg / 1000.f, 2) + "ms " +
				   " Draw: " + ofToString(wastedTimeDrawingAvg / 1000.f, 2) + "ms ",
				   x, offset + y - charH * 0.12);
	}

	if (freeze) {
		if(currentFrameNum%5 < 4) ofSetColor(frozenColor);
		else ofSetColor(ofColor::white);
		drawString("Frozen! 'F'", x + totalW - 13 * charW, y + charH );
	}

	{//lines
		ofSetColor(hilightColor);
		ofMesh lines;
		ofSetLineWidth(0.1);
		lines.setMode(OF_PRIMITIVE_LINES);
		float fuzzyFix = 0.5;
		float yy = y+1 + fuzzyFix;
		lines.addVertex(ofVec2f(x, yy));
		lines.addVertex(ofVec2f(x + totalW, yy));
		yy = y + totalH - charH - 3 + fuzzyFix;
		lines.addVertex(ofVec2f(x, yy));
		lines.addVertex(ofVec2f(x + totalW, yy));
		yy = y + totalH + fuzzyFix;
		lines.addVertex(ofVec2f(x, yy));
		lines.addVertex(ofVec2f(x + totalW, yy));
		if(menuActive){
			yy = y + totalH + fuzzyFix - (numInstructionLines + 1) * charH - 3;
			lines.addVertex(ofVec2f(x, yy));
			lines.addVertex(ofVec2f(x + totalW, yy));
			yy = y + totalH + fuzzyFix - (numInstructionLines) * charH - 3;
			lines.addVertex(ofVec2f(x, yy));
			lines.addVertex(ofVec2f(x + totalW, yy));
		}
		lines.draw();
	}//lines

	//print bottom line, fps and stuff
	float fr = ofGetFrameRate();
	bool missingFrames = ( fr < desiredFrameRate - 1.0 ); // tolerance of 1 fps TODO!
	static char msg[128];

	sprintf(msg, "%2.1f fps % 5.1f%%", fr, percentTotal );
	if(missingFrames){
		ofSetColor(170,33,33); //reddish fps below desired fps
	}else{
		ofSetColor(hilightColor);
	}
	int len = strlen(msg);
	string pad = " ";
	int diff = (maxW - len) - 1;
	for(int i = 0; i < diff; i++) pad += " ";
	int lastLine = ( drawLines.size() + 1 ) * charH + 2;
	drawString( pad + msg, x, y + lastLine );
	
	//show activate menu key
	if(menuActive ) ofSetColor(hilightColor.getInverted());
	else ofSetColor(hilightColor);
	drawString(" '" + ofToString(char(activateKey)) + "'", x, y + lastLine);

	//show averaging warning
	if (averaging) {
		if (currentFrameNum % 5 < 2) ofSetColor(hilightColor);
		else ofSetColor(ofColor::limeGreen);
		drawString(" avg!", x + charW * 3.5, y + lastLine);
	}

	for(int i = 0; i < toResetUpdatedLastFrameFlag.size(); i++){
		toResetUpdatedLastFrameFlag[i]->updatedLastFrame = false;
	}
	ofPopMatrix();
	ofPopStyle();

	if(internalBenchmark){
		wastedTimeDrawingThisFrame += TM_GET_MICROS() - timeNow;
		wastedTimeAvg = wastedTimeThisFrame * 0.025f + 0.975f * wastedTimeAvg;
		wastedTimeDrawingAvg = wastedTimeDrawingThisFrame * 0.025f + 0.975f * wastedTimeDrawingAvg;
		wastedTimeThisFrame = wastedTimeDrawingThisFrame = 0;
	}
}

#if defined(USE_OFX_HISTORYPLOT)
ofxHistoryPlot* ofxTimeMeasurements::makeNewPlot(string name){

	ofxHistoryPlot * plot = new ofxHistoryPlot( NULL, name, ofGetWidth() * plotResolution, false);
	int colorID = numAllocatdPlots%(threadColorTable.size());
	plot->setColor( threadColorTable[colorID] * 1.7 );
	plot->setBackgroundColor(ofColor(0,220));
	plot->setShowNumericalInfo(true);
	plot->setRespectBorders(true);
	plot->setLineWidth(1);
	plot->setLowerRange(0);
	plot->addHorizontalGuide(1000.0f/desiredFrameRate, ofColor(0,255,0));
	plot->setDrawGrid(true);
	plot->setGridUnit(16);
	plot->setGridColor(ofColor(20,255));
	plot->setAutoRangeShrinksBack(true);
	plot->setShowSmoothedCurve(true);
	plot->setSmoothFilter(0.03);
	plot->setDrawFromRight(true);
	numAllocatdPlots++;
	return plot;
}
#endif

bool ofxTimeMeasurements::_keyPressed(ofKeyEventArgs &e){

	if (e.key == enableKey){
		TIME_SAMPLE_SET_ENABLED(!TIME_SAMPLE_GET_ENABLED());
	}

	if (TIME_SAMPLE_GET_ENABLED()){
		if (e.key == activateKey){
			menuActive = !menuActive;
		}

		if(e.key == 'A') averaging ^= true;  //Average Toggle
		if(e.key == 'B') internalBenchmark ^= true;  //internalBenchmark Toggle
		if (e.key == 'F') freeze ^= true;  //free measurements
		

		if(e.key == 'L'){
			drawLocation = ofxTMDrawLocation(drawLocation+1);
			if(drawLocation == TIME_MEASUREMENTS_NUM_DRAW_LOCATIONS) drawLocation = ofxTMDrawLocation(0);
		}

		if(menuActive){

			if (drawLines.size()){
				int selIndex = -1;
				for(int i = 0; i < drawLines.size(); i++){
					if (drawLines[i].key == selection) selIndex = i;
				}
				if(selIndex != -1){
					switch (e.key) {

						case OF_KEY_DOWN:{
							selIndex ++;
							if(selIndex >= drawLines.size()) selIndex = 0;
							while(drawLines[selIndex].tm == NULL){
								selIndex ++;
								if(selIndex >= drawLines.size()) selIndex = 0;
							}
							selection = drawLines[selIndex].key;
						}break;

						case OF_KEY_UP:{
							selIndex --;
							if(selIndex < 0 ) selIndex = drawLines.size() - 1;
							while(drawLines[selIndex].tm == NULL){
								selIndex --;
								if(selIndex < 0 ) selIndex = drawLines.size() - 1;
							}
							selection = drawLines[selIndex].key;
						}break;

						#if defined(USE_OFX_HISTORYPLOT)
						case 'P':{
							if (!plots[selection]){
								plots[selection] = makeNewPlot(selection);
								times[selection]->settings.plotting = true;
							}else{
								times[selection]->settings.plotting ^= true;
							}
						}break;
						#endif

						case OF_KEY_RETURN:{
								//cant disable update() & draw()
								if (selection != TIME_MEASUREMENTS_SETUP_KEY &&
									selection != TIME_MEASUREMENTS_UPDATE_KEY &&
									selection != TIME_MEASUREMENTS_DRAW_KEY &&
									drawLines[selIndex].tm
									){
										times[selection]->settings.enabled ^= true;
								}
							}break;

						case OF_KEY_RIGHT:
							collapseExpand(selection, false); //expand
						break;

						case OF_KEY_LEFT:
							collapseExpand(selection, true ); //collapse
							break;
					}
				}
			}
		}
		bool ret = menuActive && e.key != OF_KEY_ESC; //return true or false; if returning false, it stops the event chain
		//so, next listerners will not get notified
		if(ret == true){
			stopMeasuring(TIME_MEASUREMENTS_KEYPRESSED_KEY, false); //if enabling the menu, we interrupt the following events,
																	//so we manually stop the timing as otherwise its never stopped
																	//bc the "after" kepressed event is never reached.
		}
		return ret;
	}
	return false; //if TM is disabled, dont interrtup event chain
}


void ofxTimeMeasurements::collapseExpand(string sel, bool collapse){

	unordered_map<ThreadId, ThreadInfo>::iterator ii;

	for( ii = threadInfo.begin(); ii != threadInfo.end(); ++ii ){

		core::tree<string> &tr = ii->second.tree;
		core::tree<string>::iterator loc = tr.tree_find_depth(sel);

		if( loc != tr.end()) {
			vector<string> subTree;
			walkTree(loc, 0, subTree);
			for(int p = 0; p < subTree.size(); p++ ){
				times[subTree[p]]->settings.visible = !collapse;
			}
		}
	}
}

string ofxTimeMeasurements::formatTime(uint64_t microSeconds, int precision){

	float time = microSeconds / 1000.0f; //to ms
	string timeUnit = "ms";
	if (time > 1000.0f){ //if more than 1 sec
		time /= 1000.0f;
		timeUnit = "sec";
		if(time > 60.0f){ //if more than a minute
			if(time > 3600.0f){ //if more than a minute
				time /= 3600.0f;
				timeUnit = "hrs";
			}else{
				time /= 60.0f;
				timeUnit = "min";
			}
		}
	}
	return ofToString(time,  precision) + timeUnit;
}


string ofxTimeMeasurements::getTimeStringForTM(TimeMeasurement* tm) {

	float time;
	if (tm->measuring){
		string anim;
		switch ((int(currentFrameNum * 0.2f))%6) {
			case 0: anim = "   "; break;
			case 1: anim = ".  "; break;
			case 2: anim = ".. "; break;
			case 3: anim = "..."; break;
			case 4: anim = " .."; break;
			case 5: anim = "  ."; break;
		}
		return string((currentFrameNum % 6 < 3 ) ? " >  " : "    ") +
				formatTime( TM_GET_MICROS() - tm->microsecondsStart, 1) +
				anim;
	}else{

		string timeString;
		static char percentChar[64];

		if (!tm->settings.enabled){
			if(tm->ifClause){
				return "   DISABLED!";
			}else{
				return "  CANT DISABLE!";
			}
		}else{

			if(tm->accumulating){
				timeString = formatTime(tm->microsecondsAccum, msPrecision);
				time = tm->microsecondsAccum / 1000.0f;
			}else{
				timeString = formatTime(tm->avgDuration, msPrecision);
				time = tm->avgDuration / 1000.0f;
			}

			float percent = 100.0f * time / (1000.0f / desiredFrameRate);
			bool over = false;
			if (percent > 100.0f){
				percent = 100.0f;
				over = true;
			}
			int originalLen = timeString.length();

			int expectedLen = 8;
			for(int i = 0; i < expectedLen - originalLen; i++){
				timeString = " " + timeString;
			}

			if (over){
				sprintf(percentChar, int(currentFrameNum * 0.8)%5 < 3  ? " >100": "  100");
			}else{
				sprintf(percentChar, "% 5.1f", percent);
			}
		}
		return timeString + percentChar + "%";
	}
}


void ofxTimeMeasurements::loadSettings(){

	//todo this might get called before OF is setup, os ofToDataPath gives us weird results sometimes?
	string f = ofToDataPath(TIME_MEASUREMENTS_SETTINGS_FILENAME, true);
	ifstream myfile(f.c_str());
	string name, visible, enabled_, plotting;
	bool fileHasPlotData = false;
	if (myfile.is_open()){

		int c = 0;
		while( !myfile.eof() ){

			if (c == 0){ //see if file has PlotData, 2 '|' per line if it does, only 1 if it doesnt
				string wholeLine;
				getline( myfile, wholeLine, '\n' );//see what version we are on
				int numBars = 0;
				for(int i = 0; i < wholeLine.size(); i++){
					if (wholeLine[i] == '|') numBars++;
				}
				if(numBars == 2) fileHasPlotData = true;
				myfile.clear();
				myfile.seekg(0, ios::beg);
				c++;
			}

			getline( myfile, name, '=' );//name
			getline( myfile, visible, '|' ); //visible
			if(fileHasPlotData){
				getline( myfile, enabled_, '|' ); //enabled
				getline( myfile, plotting, '\n' ); //enabled
			}else{
				getline( myfile, enabled_, '\n' ); //enabled
			}

			if (name == TIME_MEASUREMENTS_SETUP_KEY ||
				name == TIME_MEASUREMENTS_UPDATE_KEY ||
				name == TIME_MEASUREMENTS_DRAW_KEY ){
				visible = enabled_ = "1";
			}
			if(name.length()){
				settings[name].visible = bool(visible == "1" ? true : false);
				settings[name].enabled = bool(enabled_ == "1" ? true : false);
				#if defined(USE_OFX_HISTORYPLOT)
				settings[name].plotting = bool(plotting == "1" ? true : false);
				if(settings[name].plotting){
					ofxHistoryPlot * plot = makeNewPlot(name);
					plots[name] = plot;
				}
				#endif
			}
		}
		myfile.close();
	}else{
		ofLogWarning() << "Unable to load Settings file " << TIME_MEASUREMENTS_SETTINGS_FILENAME;
	}
}


void ofxTimeMeasurements::saveSettings(){

	if(!ofDirectory::doesDirectoryExist(configsDir)){
		ofDirectory::createDirectory(configsDir, true, true);
	}
	ofstream myfile;
	myfile.open(ofToDataPath(TIME_MEASUREMENTS_SETTINGS_FILENAME,true).c_str());
	for( unordered_map<string, TimeMeasurement*>::iterator ii = times.begin(); ii != times.end(); ++ii ){
		string keyName = ii->first;
		bool visible = times[keyName]->settings.visible;
		bool enabled = times[keyName]->settings.enabled;
		#if defined(USE_OFX_HISTORYPLOT)
		bool plotting = times[keyName]->settings.plotting;
		#endif

		if (keyName == TIME_MEASUREMENTS_SETUP_KEY ||
			keyName == TIME_MEASUREMENTS_UPDATE_KEY ||
			keyName == TIME_MEASUREMENTS_DRAW_KEY){
			visible = enabled = true;
		}

		myfile << keyName << "=" << string(visible ? "1" : "0") << "|" << string(enabled ? "1" : "0")
		#if defined(USE_OFX_HISTORYPLOT)
		<< "|" << string(plotting ? "1" : "0")
		#endif
		<< endl;
	}
	myfile.close();
}


void ofxTimeMeasurements::walkTree(core::tree<string>::iterator Arg, int levelArg, vector<string> &result){
	levelArg++;
	for(core::tree<string>::iterator x = Arg.begin(); x != Arg.end(); ++x){
		result.push_back(x.data());
		walkTree(x, levelArg, result);
	}
}


void ofxTimeMeasurements::_appExited(ofEventArgs &e){
	saveSettings();
}

#ifdef USE_OFX_FONTSTASH
void ofxTimeMeasurements::drawUiWithFontStash(string fontPath, float fontSize_){
	useFontStash = true;
	fontSize = fontSize_;
	font.setup(ofToDataPath(fontPath, true), 1.0, 512, false, 0, uiScale);
	ofRectangle r = font.getBBox("M", fontSize, 0, 0);
	charW = r.width;
	charH = ceil(r.height * 1.55);
}

void ofxTimeMeasurements::drawUiWithBitmapFont(){
	useFontStash = false;
	charW = 8;
	charH = TIME_MEASUREMENTS_LINE_HEIGHT;
}
#endif

void ofxTimeMeasurements::drawString(const string & text, const float & x, const float & y){
	#ifdef USE_OFX_FONTSTASH
	if(useFontStash){
		font.draw(text, fontSize, x + 2, y - 1);
	}else{
		ofDrawBitmapString(text, x, y);
	}
	#else
	ofDrawBitmapString(text, x, y);
	#endif
}


float ofxTimeMeasurements::durationForID( const string & ID){

	unordered_map<string,TimeMeasurement*>::iterator it;
	it = times.find(ID);

	if ( it == times.end() ){	//not found!
		if ( times[ID]->error ){
			return times[ID]->duration / 1000.0; //to ms
		}
	}
	return 0;
}


void ofxTimeMeasurements::setTimeAveragePercent(double p){
	if(p > 1.0) p = 1.0;
	if(p < 0.0) p = 0.0;
	if(p >= 0.99999f){
		averaging = false;
	}else{
		averaging = true;
		timeAveragePercent = p;
	}
}


void ofxTimeMeasurements::setDesiredFrameRate(float fr){
	desiredFrameRate = fr;
}


void ofxTimeMeasurements::setEnabled(bool ena){
	enabled = ena;
}


bool ofxTimeMeasurements::getEnabled(){
	return enabled;
}


void ofxTimeMeasurements::setMsPrecision(int digits){
	msPrecision = digits;
}


float ofxTimeMeasurements::getWidth() const{
	#ifdef USE_OFX_FONTSTASH
	return (maxW ) * charW + float(useFontStash ? 4.0f: 0.0f);
	#else
	return (maxW + 1) * charW;
	#endif
}
