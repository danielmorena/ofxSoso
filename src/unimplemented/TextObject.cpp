#include "soso/TextObject.h"
#include "cinder/ColorA8u.h"

using namespace soso;

//class TextObjectFont

TextFont::TextFont(string iFontName, SosoTrueTypeFont *iFont)
{
	font = iFont;
	fontName = strdup((char *)iFontName.c_str());
}

TextFont::TextFont(string iFontName, string iFilename, int iFontsize, bool iAntiAliased, bool iFullCharacterSet, bool iMakeContours, bool iMakeMipMaps)
{
	font = new SosoTrueTypeFont();
	font->loadFont(iFilename, iFontsize, iAntiAliased, iFullCharacterSet, iMakeContours, iMakeMipMaps);
	fontName = strdup((char *)iFontName.c_str());
}

TextFont::~TextFont()
{
	if (font) delete(font);
	if (fontName) delete(fontName);
}

//class TextObject

vector<TextFont *> TextObject::allFonts;

TextObject::TextObject(SosoTrueTypeFont *iFont, char *iString)
{
    //NOTE pointsize and leading should always be explicity set after creating text object
    pointSize = iFont->getSize();       //pointSize defaults to resolution of font
    leading = (14.0/12.0) * pointSize;  //leading defaults to 14/12 proportion

    init(iFont);

    setString(iString);

    //make these huge by default so there is not wrapping
	//PEND add wrapping switch
	columnWidth = 1920000;
	columnHeight = 1080000;

    drawAsShapes = false;
    alignment = OF_TEXT_ALIGN_LEFT;
    drawWordColor = false;  //eg

	renderDirty = true;
	isDisplayListEnabled = true;

    //this is so that text will be z-sorted by Scene as a transparent object
	setSpecialTransparency(true);
}

TextObject::TextObject(SosoTrueTypeFont *iFont, string iString) //LM 063012
{
	TextObject(iFont, (char*)iString.c_str());
}

TextObject::~TextObject()
{
	//PEND write this! BUT DON'T delete font (it's passed in from the outside)

    words.clear();
    lines.clear();
}

void TextObject::init(SosoTrueTypeFont *iFont)
{
    font = iFont;

    //scalefactor is used to make pointsize agnostic to font resolution
    scaleFactor = pointSize/font->getSize();

 	//Set up the blank space word
    blankSpaceWord.rawWord = " ";
    blankSpaceWord.width   = font->stringWidth ("m")/3.0;
	//blankSpaceWord.width   = (float)font.getFontSize()/3.0f;	//soso - default word spacing = 1/3 em
    blankSpaceWord.height  = font->stringHeight("i");
    blankSpaceWord.color.r = blankSpaceWord.color.g = blankSpaceWord.color.b = 255;
	blankSpaceWord.isNewLine = false; //soso
	blankSpaceWord.font = font;	//soso

	//soso - set up the new line word
	newLineWord.rawWord = "\n";
	newLineWord.width   = 0;
    newLineWord.height  = font->stringHeight("i");
    newLineWord.color.r = newLineWord.color.g = newLineWord.color.b = 255;
	newLineWord.isNewLine = true; //soso
	newLineWord.font = font;	//soso

}

void TextObject::render()
{
	if(isDisplayListEnabled){
		if (renderDirty) {

			glDeleteLists(displayList, 1);
			glNewList(displayList, GL_COMPILE_AND_EXECUTE);

            drawText();

			glEndList();
			renderDirty = false;
			glCallList(displayList);

		}else{
			glCallList(displayList);
		}
	}else{
        drawText();
	}
}



void TextObject::enableDisplayList(bool iEnable)
{
	isDisplayListEnabled = iEnable;
}



void TextObject::setString(char *iString)
{
    rawText = iString;	//PEND does string clean itself up when you use the = operator to reassign it?

	//replace any Named Entities (i.e. &amp;) within the [0,255] range with their appropriate unicode characters
	SosoTrueTypeFont::replaceNamedEntities(rawText);

	_loadWords();

	wrapTextX(columnWidth);
	renderDirty = true;
}

void TextObject::setString(string iString)
{
	setString((char*)iString.c_str());
}

string TextObject::getLineString(int iIndex)
{
    string lineString = "";
    if (iIndex < lines.size())
    {
        for (int i=0; i<lines[iIndex].wordsID.size(); i++)
        {
            lineString += words[lines[iIndex].wordsID[i]].rawWord;
        }
    }
    return lineString;
}

//with scale of TextObject set to 1.0, this is the pointSize of the text
void TextObject::setPointSize(float iPointSize)
{
    pointSize = iPointSize;
    //scale factor is used to make the pointsize agnostic to the font resolution
    scaleFactor = iPointSize/font->getSize();

    wrapTextX(columnWidth);
}

float TextObject::getPointSize()
{
    return pointSize;
}

float TextObject::getScaleFactor()
{
    return scaleFactor;
}

void TextObject::setColumnWidth(float iWidth)
{
	columnWidth = iWidth;
    wrapTextX(columnWidth);
}

//Doesn't exactly fix the width and height, does it based on area?
void TextObject::setColumnBox(float iWidth, float iHeight)
{
	columnWidth = iWidth;
	columnHeight = iHeight;
	wrapTextArea(columnWidth, columnHeight);
}

//Call with OF_TEXT_ALIGN_LEFT, OF_TEXT_ALIGN_RIGHT, OF_TEXT_ALIGN_JUSTIFIED, OF_TEXT_ALIGN_CENTER
void TextObject::setAlignment(TextObjectAlignment iAlignment)
{
	alignment = iAlignment;
	wrapTextX(columnWidth);
}

//iLeading is in pixels
//NOTE we are ignoring lineHeight in font because we're doing all the line formatting ourselves
void TextObject::setLeading(float iLeading)
{
    leading = iLeading;
    wrapTextX(columnWidth);
}

void TextObject::setSpaceWidth(float iWidth)
{
    blankSpaceWord.width = iWidth;
	for(unsigned int i=0; i < words.size(); i++){
		if(words[i].rawWord == blankSpaceWord.rawWord){
			words[i].width = iWidth;
		}
	}
	wrapTextX(columnWidth);

}

void TextObject::setWordColor(int iIndex, float iR, float iG, float iB, float iA)
{
    if(iIndex < words.size()){
        words[iIndex].color.r = iR;
        words[iIndex].color.g = iG;
        words[iIndex].color.b = iB;
        words[iIndex].color.a = iA;

		//drawWordColor = true; //eg - you have to manually set this now
        renderDirty = true;
    }
}

void TextObject::setWordColor(string iWord, float iR, float iG, float iB, float iA, bool iExactMatch)
{

    for (int i=0; i<words.size(); i++)
    {
        if (iExactMatch)
        {
            if (words[i].rawWord.compare(iWord) == 0)
            {
                words[i].color.r = iR;
                words[i].color.g = iG;
                words[i].color.b = iB;
                words[i].color.a = iA;
            }
        }
        else if (words[i].rawWord.find(iWord) != -1)
        {
            words[i].color.r = iR;
            words[i].color.g = iG;
            words[i].color.b = iB;
            words[i].color.a = iA;
        }

    }

    //drawWordColor = true; //eg - you have to manually set this now
    renderDirty = true;

}

void TextObject::setAllWordsColor(float iR,  float iG, float iB, float iA)
{
    ci::ColorA8u tmpColor;
    tmpColor.r = iR;
    tmpColor.g = iG;
    tmpColor.b = iB;
    tmpColor.a = iA;

    if (words.size() > 0) {
        for(int i=0;i < words.size(); i++)
            words[i].color = tmpColor;

		//drawWordColor = true; //eg - you have to manually set this now
    }

}

//You have to call this with true to enable per word coloring
void TextObject::enableWordColoring(bool iFlag)
{
    drawWordColor = iFlag;
    if(drawWordColor) enableDisplayList(false); //When word coloring is being used, display lists will break alpha calculations (since they happen inside of render).
}

void TextObject::setColor(float iR, float iG, float iB, float iA)
{
    Object::setColor(iR, iG, iB, iA);
    setAllWordsColor(iR, iG, iB, iA);

    renderDirty = true;
}

void TextObject::setColor(ci::Vec4f color)
{
    Object::setColor(color.x, color.y, color.z, color.w);
    setAllWordsColor(color.x, color.y, color.z, color.w);

    renderDirty = true;
}

void TextObject::setColor(ci::ColorA8u color)
{
    Object::setColor(color.r, color.g, color.b, color.a);
    setAllWordsColor(color.r, color.g, color.b, color.a);

    renderDirty = true;
}

void TextObject::setAlpha(float iA)
{
	Object::setAlpha(iA);
	renderDirty = true;
}

void TextObject::setFont(SosoTrueTypeFont *iFont)
{
    init(iFont);

    renderDirty = true;
}

int TextObject::getNumLines()
{
    return lines.size();
}


float TextObject::getWidth()
{
    int   currentWordID;

    float currX = 0.0f;
    float maxWidth  = 0.0f;

    if (words.size() > 0) {

        for(int l=0;l < lines.size(); l++)
        {
            for(int w=0;w < lines[l].wordsID.size(); w++)
            {
                currentWordID = lines[l].wordsID[w];
                currX += words[currentWordID].width;
            }
            maxWidth = MAX(maxWidth, currX);
            currX = 0.0f;
        }
        //return maxWidth * scale;
		return maxWidth/getPointSize();
    }
    else return 0;
}

float TextObject::getHeight(){

    if (words.size() > 0) {
        return leading * lines.size();	//soso
    }
    else return 0;
}

float TextObject::getColumnWidth()
{
	return columnWidth;
}

ci::Vec2f TextObject::getCharPosition(int iIndex) {
    if (iIndex >= 0 && iIndex < rawText.size()) {
        int j = iIndex;
        for (int i=0; i<words.size(); i++) {
            //printf("i:%d word:%s j:%d letters:%d\n", i, (char*)words[i].rawWord.c_str(), j, (int)words[i].charPositions.size());
            if (words[i].charPositions.size() > 0) {
                if (words[i].charPositions.size() < j) {
                    j -= words[i].charPositions.size()+1;
                }
                else {
                    ci::Vec2f pos = words[i].charPositions[j] + words[i].pos;

                    //cout << "found pos " << words[i].rawWord.at(j) << " in string " << rawText << " x:" << pos.x << " y:" << pos.y << "\n";
                    return scaleFactor*(words[i].pos + words[i].charPositions[j]);
                }
            }
        }
    } else printf("TextObject::getCharPosition - index out of bounds\n");
}


SosoTrueTypeFont* TextObject::getFont()
{
    return font;
}


float TextObject::getLeading()
{
    return leading;
}


char* TextObject::getString()
{
    return (char *)rawText.c_str();
}

int TextObject::wrapTextX(float lineWidth)
{
	columnWidth = lineWidth; //soso

    if (words.size() > 0) {

        float   runningWidth = 0.0f;

        lines.clear();

        bool        newLine = true;
        lineBlock   tmpLine;
        tmpLine.wordsID.clear();
        int         activeLine = 0;

        for(int i=0;i < words.size(); i++)
        {
            runningWidth += (words[i].width);


            //soso - check for new line word
			if (words[i].isNewLine)
			{
				//printf("New Line \n");
				newLine = true;
                lines.push_back(tmpLine);
                tmpLine.wordsID.clear();
                //runningWidth = 0.0f + words[i].width;
                runningWidth = 0.0f + scaleFactor*words[i].width;   //Account for scaleFactor.
                activeLine++;
			}
			//else if (runningWidth <= (lineWidth)) {
            else if ((scaleFactor*runningWidth) <= lineWidth) {	  //Account for scaleFactor here when wrapping lines.
                newLine = false;
            }
            else {
				//printf("Wrap New Line \n");
                newLine = true;
                lines.push_back(tmpLine);
                tmpLine.wordsID.clear();
                runningWidth = 0.0f + words[i].width;
                activeLine++;
            }

            tmpLine.wordsID.push_back(i);
        }

        //Push in the final line.
        lines.push_back(tmpLine);
        _trimLineSpaces(); //Trim the leading and trailing spaces.
    }

	updatePositions(lineWidth);	//All other wrap calls eventually boil down to this wrap method, so we only have to call this once here.

    renderDirty = true;

    return lines.size();

}

//PEND not updated to new system yet
void TextObject::wrapTextArea(float rWidth, float rHeight)
{
    float tmpScale = 0.0f;
    float maxIterations = _getLinedWords();
    float scales[1000];
    //scale = 1.0f;  //Reset the scale for the height and width calculations.	//soso

	columnWidth = rWidth; //soso

    if (words.size() > 0) {

        //Check each possible line layout and check it will fit vertically
        for (int iteration=1; iteration <= maxIterations; iteration++){

            //printf("Iteration %i...\n", iteration);
            wrapTextForceLines(iteration);

            tmpScale = rWidth / getWidth();
            if ((tmpScale * getHeight()) < rHeight) {
                scales[iteration] = tmpScale;
            }
            else {
                scales[iteration] = -1;
            }
        }

        //Now see which is biggest
        int maxIndex = 1;
        bool bScaleAvailable = false;

        for (int i=1; i <= maxIterations; i++) {
            ofLog(OF_LOG_VERBOSE,"Scales %i = %f\n", i, scales[maxIndex]);
            if (scales[i] != -1) bScaleAvailable = true;

            if (scales[i] > scales[maxIndex]) {
                maxIndex = i;
            }
        }

		/*	//soso
         //When only one line is needed an appropriate on the Y scale can sometimes not be found.  In these occasions scale the size to the Y dimension
         if (bScaleAvailable) {
         scale = scales[maxIndex];
         }
         else {
         scale = (float)rHeight / (float)getHeight();
         }
         */

        //float persistScale = scale; //Need to persist the scale as the wrapTextForceLines will overwrite.	//soso
        wrapTextForceLines(maxIndex);
        //scale = persistScale;	//soso

        //ofLog(OF_LOG_VERBOSE,"Scaling with %i at scale %f...\n", maxIndex, scale);	//soso
    }

    renderDirty = true;
}


//PEND not updated to new system yet
bool TextObject::wrapTextForceLines(int linesN)
{
    if (words.size() > 0) {

        if (linesN > words.size()) linesN = words.size();

        float lineWidth = _getWidthOfWords() * (1.1f / (float)linesN);

        int curLines = 0;
        bool bGotLines = false;	//soso

        //keep increasing the line width until we get the desired number of lines.
        while (!bGotLines) {

            //curLines = wrapTextX(lineWidth);
            curLines = wrapTextX(lineWidth);
            if (curLines == linesN) return true;
            if (curLines > linesN) return false;
            lineWidth-=10;
			columnWidth = lineWidth; //soso

        }

        renderDirty = true;
		return true;	//soso
    }
	else return false; //soso
}

//For updating word and line positions, based on alignment.
void TextObject::updatePositions(float width)
{
	//Call appropriate draw (with drawFlag=false) to update word positions.
	switch(alignment){
		case OF_TEXT_ALIGN_LEFT:
			drawLeft(0,0,false);
			break;
		case OF_TEXT_ALIGN_RIGHT:
			drawRight(0,0,false);
			break;
		case OF_TEXT_ALIGN_JUSTIFIED:
			drawJustified(0,0,false);	//calls with fake value you have to call a wrap AFTER you call setAlignment()
			break;
		case OF_TEXT_ALIGN_CENTER:
			drawCenter(0,0,false);
			break;
	};
}


//The following draw methods were adapted from Luke Malcom's Text Suite ofAddon
void TextObject::drawText(float x, float y, bool drawFlag)
{
	if (alignment == OF_TEXT_ALIGN_LEFT) {
	    drawLeft(x, y, drawFlag);
	} else if (alignment == OF_TEXT_ALIGN_RIGHT) {
		drawRight(x, y, drawFlag);
	} else if (alignment == OF_TEXT_ALIGN_CENTER) {
		drawCenter(x, y, drawFlag);
	} else if (alignment == OF_TEXT_ALIGN_JUSTIFIED) {
		drawJustified(x, y, drawFlag);
	}
}



void TextObject::drawLeft(float x, float y, bool drawFlag)
{
    string  strToDraw;
    int     currentWordID;
    float   drawX = 0;
    float   drawY = 0;

    float currX = 0;

    if (words.size() > 0) {

        if(drawFlag){
            glPushMatrix();
            glScalef(scaleFactor, scaleFactor, 1.0);
        }

        for(int l=0;l < lines.size(); l++)
        {
            for(int w=0;w < lines[l].wordsID.size(); w++)
            {
                currentWordID = lines[l].wordsID[w];

                drawX = x + currX;
                drawY = y - (leading/scaleFactor * float(l));

				words[currentWordID].pos.set(drawX, drawY); //Record word position

				if(drawFlag){
					if (drawWordColor){
						ofSetColor(words[currentWordID].color.r, words[currentWordID].color.g, words[currentWordID].color.b, words[currentWordID].color.a * drawMaterial->color.w/255.0f);
						//printf("word %d color is %f, %f, %f\n", currentWordID, words[currentWordID].color.r, words[currentWordID].color.g, words[currentWordID].color.b, words[currentWordID].color.a);
					}

					if(!drawAsShapes){
						words[currentWordID].font->drawString(words[currentWordID].rawWord.c_str(), drawX, drawY);
					}else{ //Support for vector drawing
						words[currentWordID].font->drawStringAsShapes(words[currentWordID].rawWord.c_str(), drawX, drawY);
					}
				}
				words[currentWordID].charPositions = words[currentWordID].font->getCharPositions(words[currentWordID].rawWord.c_str());	//Store char positions for word.
                currX += words[currentWordID].width;

            }
            currX = 0;
			lines[l].pos.set(currX, drawY);	//Record line position
        }
        if(drawFlag){
            glPopMatrix();
        }
    }
}

void TextObject::drawCenter(float x, float y, bool drawFlag)
{
    string  strToDraw;
    int     currentWordID;
    float   drawX = 0;
    float   drawY = 0;
    float   lineWidth;

    float currX = 0;

    if (words.size() > 0) {

		if(drawFlag){
            glPushMatrix();
            glScalef(scaleFactor, scaleFactor, 1.0);
        }

        for(int l=0;l < lines.size(); l++)
        {

            //Get the length of the line.
            lineWidth = 0;
            for(int w=0;w < lines[l].wordsID.size(); w++)
            {
                currentWordID = lines[l].wordsID[w];
                lineWidth += words[currentWordID].width;
            }

            for(int w=0;w < lines[l].wordsID.size(); w++)
            {
                currentWordID = lines[l].wordsID[w];

                drawX = -(lineWidth / 2) + currX;
				drawY = y - (leading/scaleFactor * float(l));


				words[currentWordID].pos.set(drawX, drawY); //Record word position.

				if(drawFlag){
					if (drawWordColor) ofSetColor(words[currentWordID].color.r, words[currentWordID].color.g, words[currentWordID].color.b, words[currentWordID].color.a * drawMaterial->color.w/255.0f);

					if(!drawAsShapes){
						words[currentWordID].font->drawString(words[currentWordID].rawWord.c_str(), drawX, drawY);
					}else{ //Support for vector drawing
						words[currentWordID].font->drawStringAsShapes(words[currentWordID].rawWord.c_str(), drawX, drawY);
					}
				}
				words[currentWordID].charPositions = words[currentWordID].font->getCharPositions(words[currentWordID].rawWord.c_str());	//Store char positions for word.
                currX += words[currentWordID].width;

            }
            currX = 0;
			lines[l].pos.set(currX, drawY);	//Record line position.
        }
		if(drawFlag){
            glPopMatrix();
        }
    }
}




void TextObject::drawRight(float x, float y, bool drawFlag)
{
    string  strToDraw;
    int     currentWordID;
    float   drawX = 0;
    float   drawY = 0;

    float currX = 0;

    if (words.size() > 0) {

		if(drawFlag){
            glPushMatrix();
            glScalef(scaleFactor, scaleFactor, 1.0);
        }

        for(int l=0; l < lines.size(); l++)
        {
            for(int w=lines[l].wordsID.size() - 1; w >= 0; w--)
            {
                currentWordID = lines[l].wordsID[w];

				drawX = -currX - words[currentWordID].width;
				drawY = y - (leading/scaleFactor * float(l));

				words[currentWordID].pos.set(drawX, drawY); //Record word position.

				if(drawFlag){
					if (drawWordColor) ofSetColor(words[currentWordID].color.r, words[currentWordID].color.g, words[currentWordID].color.b, words[currentWordID].color.a * drawMaterial->color.w/255.0f);	//soso - removed this functionality for now //LM13 added back in..eep?

					if(!drawAsShapes){
						words[currentWordID].font->drawString(words[currentWordID].rawWord.c_str(), drawX, drawY);
					}else{ //Support for vector drawing
						words[currentWordID].font->drawStringAsShapes(words[currentWordID].rawWord.c_str(), drawX, drawY);
					}
				}
				words[currentWordID].charPositions = words[currentWordID].font->getCharPositions(words[currentWordID].rawWord.c_str());	//soso - store char positions for word (using word font)
                currX += words[currentWordID].width;

            }
            currX = 0;
			lines[l].pos.set(currX, drawY);	//Record line position.
        }

		if(drawFlag){
            glPopMatrix();
        }
    }
}

void TextObject::drawJustified(float x, float y, bool drawFlag)
{
    string  strToDraw;
    int     currentWordID;
    float   drawX = 0;
    float   drawY = 0;
    int     spacesN;
    float   nonSpaceWordWidth;
    float   pixelsPerSpace;

    float currX = 0;

    if (words.size() > 0) {

		if(drawFlag){
            glPushMatrix();
            glScalef(scaleFactor, scaleFactor, 1.0);
        }

        for(int l=0;l < lines.size(); l++)
        {
            //Find number of spaces and width of other words.
            spacesN = 0;
            nonSpaceWordWidth = 0;

            for(int w=0;w < lines[l].wordsID.size(); w++)
            {
                currentWordID = lines[l].wordsID[w];
                if (words[currentWordID].rawWord == " ") spacesN++;
                else nonSpaceWordWidth += words[currentWordID].width;
            }

			pixelsPerSpace = ((columnWidth/scaleFactor) - (x/scaleFactor) - nonSpaceWordWidth) / spacesN;

            for(int w=0;w < lines[l].wordsID.size(); w++)
            {
                currentWordID = lines[l].wordsID[w];

                drawX = currX;
                drawY = y - (leading/scaleFactor * float(l));

				words[currentWordID].pos.set(drawX, drawY); //Record word position.

				if (words[currentWordID].rawWord != " ") {
					if(drawFlag){
						if (drawWordColor) ofSetColor(words[currentWordID].color.r, words[currentWordID].color.g, words[currentWordID].color.b, words[currentWordID].color.a * drawMaterial->color.w/255.0f);	//soso - removed this functionality for now //LM13 added back in..eep?

						if(!drawAsShapes){
							words[currentWordID].font->drawString(words[currentWordID].rawWord.c_str(), drawX, drawY);
						}else{ //Support for vector drawing
							words[currentWordID].font->drawStringAsShapes(words[currentWordID].rawWord.c_str(), drawX, drawY);
						}
					}
					words[currentWordID].charPositions = words[currentWordID].font->getCharPositions(words[currentWordID].rawWord.c_str());	//soso - store char positions for word (using word font)
					currX += words[currentWordID].width;
				}
				else {
                    currX += pixelsPerSpace;
                }
            }
            currX = 0;
			lines[l].pos.set(currX, drawY);	//soso - record line position
        }
		if(drawFlag) glPopMatrix();
    }
}

void TextObject::_trimLineSpaces()
{
    if (words.size() > 0) {
        //Now delete all leading or ending spaces on each line
        for(int l=0;l < lines.size(); l++)
        {
            //Delete the first word if it is a blank
            if (lines[l].wordsID.size() > 0){
                if (words[lines[l].wordsID[0]].rawWord == " ")   lines[l].wordsID.erase(lines[l].wordsID.begin());
            }

            //Delete the last word if it is a blank
            if (lines[l].wordsID.size() > 0){
                if (words[lines[l].wordsID[lines[l].wordsID.size() - 1]].rawWord == " ") lines[l].wordsID.erase(lines[l].wordsID.end() - 1);
            }
        }
    }

}

int TextObject::_getLinedWords()
{
    int wordCount = 0;

    if (words.size() > 0) {

        for(int l=0;l < lines.size(); l++)
        {
            wordCount += lines[l].wordsID.size();
        }
        return wordCount;
    }
    else return 0;
}

//soso - load words used to delete new lines
//PEND - need to make sure we are deleting all the helper buffers
void TextObject::_loadWords()
{
	//soso - doing this the nitty gritty way
    wordBlock tmpWord;

	words.clear();

	char *rawBuf = (char *)rawText.c_str();
	int size = (int)strlen(rawBuf);

	const int maxWordSize = 500;
	char wordBuf[maxWordSize];

	int wordBufIndex = 0;

	for (int i=0; i<size; i++)
	{
		char c = rawBuf[i];

		if (c == '\n' || c == ' ') {
			//close the existing word buffer
			if (wordBufIndex > 0)
			{
				string wordString(wordBuf, wordBufIndex);

				tmpWord.rawWord = wordString;
				tmpWord.width   = font->stringWidth(tmpWord.rawWord);
				tmpWord.height  = font->stringHeight(tmpWord.rawWord);
				tmpWord.color.r = material->color.x;
				tmpWord.color.g = material->color.y;
				tmpWord.color.b = material->color.z;
				tmpWord.color.a = material->color.w;
				tmpWord.isNewLine = false;	//soso
				tmpWord.font = font;		//soso

				words.push_back(tmpWord);

				//reset
				wordBuf[0] = 0;
				wordBufIndex = 0;
			}

            if (c == '\n') {
                //add a new line
                words.push_back(newLineWord);
            }
            else {
                //add a space
                words.push_back(blankSpaceWord);
            }
		}
		else {
			if (wordBufIndex < maxWordSize - 1)
			{
				wordBuf[wordBufIndex] = c;
				wordBuf[wordBufIndex + 1] = 0;
				wordBufIndex++;
			}
		}

	}
	//close the last word
	if (wordBufIndex > 0)
	{
		string wordString(wordBuf, wordBufIndex);

		tmpWord.rawWord = wordString;
		tmpWord.width   = font->stringWidth(tmpWord.rawWord);
		tmpWord.height  = font->stringHeight(tmpWord.rawWord);
		tmpWord.color.r = material->color.x;
		tmpWord.color.g = material->color.y;
		tmpWord.color.b = material->color.z;
		tmpWord.color.a = material->color.w;
		tmpWord.isNewLine = false; //soso
		tmpWord.font = font;		//soso

		words.push_back(tmpWord);

		//reset
		wordBuf[0] = 0;
		wordBufIndex = 0;
	}



    for(int i=0;i < words.size(); i++)
    {
        ofLog(OF_LOG_VERBOSE, "Loaded word: %i, %s\n", i, words[i].rawWord.c_str());
    }


}

//helper function for
void TextObject::_checkString(string & inputString, char *checkBuf, string checkString)
{
	int res = inputString.find(checkBuf);
	if (res != string::npos)
	{
		int len = strlen(checkBuf);
		inputString.replace(res, len, checkString);
	}
}



float TextObject::_getWidthOfWords()
{

    float widthTotal = 0.0f;

    if (words.size() > 0) {
        for(int i=0;i < words.size(); i++)
        {
            widthTotal += words[i].width;

        }
        return widthTotal;
    }
    else {
        return 0.0f;
    }

}


//Turns on/off shape drawing. The font has to have been made with the makeContours constructor argument set to true.
void TextObject::enableDrawAsShapes(bool iFlag)
{
	drawAsShapes = iFlag;
}

//methods for creating a static respository of fonts for everyone to access
void TextObject::addFont(TextFont *iFont)
{
	allFonts.push_back(iFont);
}


TextFont* TextObject::addFont(char *iName, SosoTrueTypeFont *iFont)
{
	TextFont *font = new TextFont(iName, iFont);
	allFonts.push_back(font);
	return font;
}

SosoTrueTypeFont* TextObject::getFont(char *iFontName)
{
	for(int i=0; i < allFonts.size(); i++){
		if(strcmp(allFonts[i]->fontName, iFontName)==0)
			return allFonts[i]->font;
	}
	return NULL;
}