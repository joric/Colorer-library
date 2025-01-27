#include "colorer/common/UStr.h"
#include "colorer/parsers/TextParserImpl.h"

TextParser::Impl::Impl()
{
  CTRACE(spdlog::trace("[TextParserImpl] constructor"));
  cache = new ParseCache();
  clearCache();
}

TextParser::Impl::~Impl()
{
  clearCache();
  delete cache;
}

void TextParser::Impl::setFileType(FileType* type)
{
  baseScheme = nullptr;
  if (type != nullptr) {
    baseScheme = (SchemeImpl*) (type->getBaseScheme());
  }
  clearCache();
}

void TextParser::Impl::setLineSource(LineSource* lh)
{
  lineSource = lh;
}

void TextParser::Impl::setRegionHandler(RegionHandler* rh)
{
  regionHandler = rh;
}

int TextParser::Impl::parse(int from, int num, TextParseMode mode)
{
  gx = 0;
  gy = from;
  gy2 = from + num;
  clearLine = -1;

  invisibleSchemesFilled = false;
  schemeStart = -1;
  breakParsing = false;
  updateCache = (mode == TextParseMode::TPM_CACHE_UPDATE);

  CTRACE(spdlog::trace("[TextParserImpl] parse from={0}, num={1}", from, num));
  /* Check for initial bad conditions */
  if (!regionHandler || !lineSource || !baseScheme) {
    return from;
  }

  vtlist = new VTList();

  lineSource->startJob(from);
  regionHandler->startParsing(from);

  /* Init cache */
  parent = cache;
  forward = nullptr;
  cache->scheme = baseScheme;

  if (mode == TextParseMode::TPM_CACHE_READ || mode == TextParseMode::TPM_CACHE_UPDATE) {
    parent = cache->searchLine(from, &forward);
    if (parent != nullptr) {
      CTRACE(spdlog::trace("[TPCache] searchLine() parent:{0},{1}-{2}", *parent->scheme->getName(), parent->sline, parent->eline));
    }
  }
  CTRACE(spdlog::trace("[TextParserImpl] parse: cache filled"));

  do {
    if (!forward) {
      if (!parent) {
        return from;
      }
      if (updateCache) {
        delete parent->children;
        parent->children = nullptr;
      }
    } else {
      if (updateCache) {
        delete forward->next;
        forward->next = nullptr;
      }
    }
    baseScheme = parent->scheme;

    stackLevel = 0;
    CTRACE(spdlog::trace("[TextParserImpl] parse: goes into colorize()"));
    if (parent != cache) {
      vtlist->restore(parent->vcache);
      parent->clender->end->setBackTrace(parent->backLine, &parent->matchstart);
      colorize(parent->clender->end.get(), parent->clender->lowContentPriority);
      vtlist->clear();
    } else {
      colorize(nullptr, false);
    }

    if (updateCache) {
      if (parent != cache) {
        parent->eline = gy;
      }
    }
    if (parent != cache && gy < gy2) {
      leaveScheme(gy, &matchend, parent->clender);
    }
    gx = matchend.e[0];

    forward = parent;
    parent = parent->parent;
  } while (parent);
  regionHandler->endParsing(endLine);
  lineSource->endJob(endLine);
  delete vtlist;
  return endLine;
}

void TextParser::Impl::clearCache()
{
  ParseCache *tmp, *tmp2;
  tmp = cache->next;
  while (tmp) {
    tmp2 = tmp->next;
    delete tmp;
    tmp = tmp2;
  }
  delete cache->children;
  delete cache->backLine;
  cache->backLine = nullptr;
  cache->sline = 0;
  cache->eline = 0x7FFFFFF;
  cache->children = cache->parent = cache->next = nullptr;
}

void TextParser::Impl::breakParse()
{
  breakParsing = true;
}

void TextParser::Impl::addRegion(int lno, int sx, int ex, const Region* region)
{
  if (sx == -1 || region == nullptr) {
    return;
  }
  regionHandler->addRegion(lno, str, sx, ex, region);
}

void TextParser::Impl::enterScheme(int lno, int sx, int ex, const Region* region)
{
  regionHandler->enterScheme(lno, str, sx, ex, region, baseScheme);
}

void TextParser::Impl::leaveScheme(int lno, int sx, int ex, const Region* region)
{
  regionHandler->leaveScheme(lno, str, sx, ex, region, baseScheme);
  if (region != nullptr) {
    picked = region;
  }
}

void TextParser::Impl::enterScheme(int lno, const SMatches* match, const SchemeNode* schemeNode)
{
  if (schemeNode->innerRegion) {
    enterScheme(lno, match->e[0], match->e[0], schemeNode->region);
  } else {
    enterScheme(lno, match->s[0], match->e[0], schemeNode->region);
  }

  for (int i = 0; i < match->cMatch; i++) {
    addRegion(lno, match->s[i], match->e[i], schemeNode->regions[i]);
  }
  for (int i = 0; i < match->cnMatch; i++) {
    addRegion(lno, match->ns[i], match->ne[i], schemeNode->regionsn[i]);
  }
}

void TextParser::Impl::leaveScheme(int /*lno*/, const SMatches* match, const SchemeNode* schemeNode)
{
  if (schemeNode->innerRegion) {
    leaveScheme(gy, match->s[0], match->s[0], schemeNode->region);
  } else {
    leaveScheme(gy, match->s[0], match->e[0], schemeNode->region);
  }

  for (int i = 0; i < match->cMatch; i++) {
    addRegion(gy, match->s[i], match->e[i], schemeNode->regione[i]);
  }
  for (int i = 0; i < match->cnMatch; i++) {
    addRegion(gy, match->ns[i], match->ne[i], schemeNode->regionen[i]);
  }
}

void TextParser::Impl::fillInvisibleSchemes(ParseCache* ch)
{
  if (!ch->parent || ch == cache) {
    return;
  }
  /* Fills output stream with valid "pseudo" enterScheme */
  fillInvisibleSchemes(ch->parent);
  enterScheme(gy, 0, 0, ch->clender->region);
}

int TextParser::Impl::searchKW(const SchemeNode* node, int /*no*/, int lowlen, int /*hilen*/)
{
  if (!node->kwList->num) {
    return MATCH_NOTHING;
  }

  if (node->kwList->minKeywordLength + gx > lowlen) {
    return MATCH_NOTHING;
  }
  if (gx < lowlen && !node->kwList->firstChar->contains((*str)[gx])) {
    return MATCH_NOTHING;
  }

  int left = 0;
  int right = node->kwList->num;
  while (true) {
    int pos = left + (right - left) / 2;
    int kwlen = node->kwList->kwList[pos].keyword->length();
    if (lowlen < gx + kwlen) {
      kwlen = lowlen - gx;
    }

    int cr;
    if (node->kwList->matchCase) {
      cr = node->kwList->kwList[pos].keyword->compare(UnicodeString(*str, gx, kwlen));
    } else {
      cr = node->kwList->kwList[pos].keyword->caseCompare(UnicodeString(*str, gx, kwlen), 0);
    }

    if (cr == 0 && right - left == 1) {
      bool badbound = false;
      if (!node->kwList->kwList[pos].isSymbol) {
        if (!node->worddiv) {
          if (gx && (UStr::isLetterOrDigit((*str)[gx - 1]) || (*str)[gx - 1] == '_')) {
            badbound = true;
          }
          if (gx + kwlen < lowlen && (UStr::isLetterOrDigit((*str)[gx + kwlen]) || (*str)[gx + kwlen] == '_')) {
            badbound = true;
          }
        } else {
          // custom check for word bound
          if (gx && !node->worddiv->contains((*str)[gx - 1])) {
            badbound = true;
          }
          if (gx + kwlen < lowlen && !node->worddiv->contains((*str)[gx + kwlen])) {
            badbound = true;
          }
        }
      }
      if (!badbound) {
        CTRACE(spdlog::trace("[TextParserImpl] KW matched. gx={0}, region={1}", gx, *node->kwList->kwList[pos].region->getName()));
        addRegion(gy, gx, gx + kwlen, node->kwList->kwList[pos].region);
        gx += kwlen;
        return MATCH_RE;
      }
    }
    if (right - left == 1) {
      left = node->kwList->kwList[pos].ssShorter;
      if (left != -1) {
        right = left + 1;
        continue;
      }
      break;
    }
    if (cr == 1) {
      right = pos;
    }
    if (cr == 0 || cr == -1) {
      left = pos;
    }
  }
  return MATCH_NOTHING;
}

int TextParser::Impl::searchRE(const SchemeImpl* cscheme, int no, int lowLen, int hiLen)
{
  int i, re_result;
  SchemeImpl* ssubst = nullptr;
  SMatches match {};
  ParseCache* OldCacheF = nullptr;
  ParseCache* OldCacheP = nullptr;
  ParseCache* ResF = nullptr;
  ParseCache* ResP = nullptr;

  CTRACE(spdlog::trace("[TextParserImpl] searchRE: entered scheme \"{0}\"", *cscheme->getName()));

  if (!cscheme) {
    return MATCH_NOTHING;
  }
  int idx = 0;
  for (auto const& schemeNode : cscheme->nodes) {
    CTRACE(spdlog::trace("[TextParserImpl] searchRE: processing node:{0}/{1}, type:{2}", idx + 1, cscheme->nodes.size(),
                         SchemeNode::schemeNodeTypeNames[static_cast<int>(schemeNode->type)]));
    switch (schemeNode->type) {
      case SchemeNode::SchemeNodeType::SNT_EMPTY:
        break;
      case SchemeNode::SchemeNodeType::SNT_INHERIT:
        if (!schemeNode->scheme) {
          break;
        }
        ssubst = vtlist->pushvirt(schemeNode->scheme);
        if (!ssubst) {
          bool b = vtlist->push(schemeNode.get());
          re_result = searchRE(schemeNode->scheme, no, lowLen, hiLen);
          if (b) {
            vtlist->pop();
          }
        } else {
          re_result = searchRE(ssubst, no, lowLen, hiLen);
          vtlist->popvirt();
        }
        if (re_result != MATCH_NOTHING) {
          return re_result;
        }
        break;

      case SchemeNode::SchemeNodeType::SNT_KEYWORDS:
        if (searchKW(schemeNode.get(), no, lowLen, hiLen) == MATCH_RE) {
          return MATCH_RE;
        }
        break;

      case SchemeNode::SchemeNodeType::SNT_RE:
        if (!schemeNode->start->parse(str, gx, schemeNode->lowPriority ? lowLen : hiLen, &match, schemeStart)) {
          break;
        }
        CTRACE(spdlog::trace("[TextParserImpl] RE matched. gx={0}", gx));
        for (i = 0; i < match.cMatch; i++) {
          addRegion(gy, match.s[i], match.e[i], schemeNode->regions[i]);
        }
        for (i = 0; i < match.cnMatch; i++) {
          addRegion(gy, match.ns[i], match.ne[i], schemeNode->regionsn[i]);
        }

        /* skips regexp if it has zero length */
        if (match.e[0] == match.s[0]) {
          break;
        }
        gx = match.e[0];
        return MATCH_RE;

      case SchemeNode::SchemeNodeType::SNT_SCHEME: {
        if (!schemeNode->scheme) {
          break;
        }
        if (!schemeNode->start->parse(str, gx, schemeNode->lowPriority ? lowLen : hiLen, &match, schemeStart)) {
          break;
        }

        CTRACE(spdlog::trace("[TextParserImpl] Scheme matched. gx={0}", gx));

        gx = match.e[0];
        ssubst = vtlist->pushvirt(schemeNode->scheme);
        if (!ssubst) {
          ssubst = schemeNode->scheme;
        }

        auto* backLine = new UnicodeString(*str);
        if (updateCache) {
          ResF = forward;
          ResP = parent;
          if (forward) {
            forward->next = new ParseCache;
            forward->next->prev = forward;
            OldCacheF = forward->next;
            OldCacheP = parent ? parent : forward->parent;
            parent = forward->next;
            forward = nullptr;
          } else {
            forward = new ParseCache;
            parent->children = forward;
            OldCacheF = forward;
            OldCacheP = parent;
            parent = forward;
            forward = nullptr;
          }
          OldCacheF->parent = OldCacheP;
          OldCacheF->sline = gy + 1;
          OldCacheF->eline = 0x7FFFFFFF;
          OldCacheF->scheme = ssubst;
          OldCacheF->matchstart = match;
          OldCacheF->clender = schemeNode.get();
          OldCacheF->backLine = backLine;
        }

        int ogy = gy;
        bool zeroLength;

        SchemeImpl* o_scheme = baseScheme;
        int o_schemeStart = schemeStart;
        SMatches o_matchend = matchend;
        SMatches* o_match;
        UnicodeString* o_str;
        schemeNode->end->getBackTrace((const UnicodeString**) &o_str, &o_match);

        baseScheme = ssubst;
        schemeStart = gx;
        schemeNode->end->setBackTrace(backLine, &match);

        enterScheme(no, &match, schemeNode.get());

        colorize(schemeNode->end.get(), schemeNode->lowContentPriority);

        if (gy < gy2) {
          leaveScheme(gy, &matchend, schemeNode.get());
        }
        gx = matchend.e[0];
        /* (empty-block.test) Check if the consumed scheme is zero-length */
        zeroLength = (match.s[0] == matchend.e[0] && ogy == gy);

        schemeNode->end->setBackTrace(o_str, o_match);
        matchend = o_matchend;
        schemeStart = o_schemeStart;
        baseScheme = o_scheme;

        if (updateCache) {
          if (ogy == gy) {
            delete OldCacheF;
            if (ResF) {
              ResF->next = nullptr;
            } else if (ResP) {
              ResP->children = nullptr;
            }
            forward = ResF;
            parent = ResP;
          } else {
            OldCacheF->eline = gy;  //-V522
            OldCacheF->vcache = vtlist->store();
            forward = OldCacheF;
            parent = OldCacheP;
          }
        } else {
          delete backLine;
        }
        if (ssubst != schemeNode->scheme) {
          vtlist->popvirt();
        }
        /* (empty-block.test) skips block if it has zero length and spread over single line */
        if (zeroLength) {
          break;
        }

        return MATCH_SCHEME;
      }
    }
    idx++;
  }
  return MATCH_NOTHING;
}

bool TextParser::Impl::colorize(CRegExp* root_end_re, bool lowContentPriority)
{
  len = -1;

  /* Direct check for recursion level */
  if (stackLevel > MAX_RECURSION_LEVEL) {
    return true;
  }
  stackLevel++;

  for (; gy < gy2;) {
    CTRACE(spdlog::trace("[TextParserImpl] colorize: line no {0}", gy));
    // clears line at start,
    // prevents multiple requests on each line
    if (clearLine != gy) {
      clearLine = gy;
      str = lineSource->getLine(gy);
      if (str == nullptr) {
        throw Exception("null String passed into the parser: " + UStr::to_unistr(gy));
      }
      regionHandler->clearLine(gy, str);
    }
    // hack to include invisible regions in start of block
    // when parsing with cache information
    if (!invisibleSchemesFilled) {
      invisibleSchemesFilled = true;
      fillInvisibleSchemes(parent);
    }
    // updates length
    if (len < 0) {
      len = str->length();
    }
    endLine = gy;

    // searches for the end of parent block
    int res = 0;
    if (root_end_re) {
      res = root_end_re->parse(str, gx, len, &matchend, schemeStart);
    }
    if (!res) {
      matchend.s[0] = matchend.e[0] = gx + maxBlockSize > len ? len : gx + maxBlockSize;
    }

    int parent_len = len;
    /*
    BUG: <regexp match="/.{3}\M$/" region="def:Error" priority="low"/>
    $ at the end of current schema
    */
    if (lowContentPriority) {
      len = matchend.s[0];
    }

    int ret = LINE_NEXT;
    for (; gx <= matchend.s[0];) {  //    '<' or '<=' ???
      if (breakParsing) {
        gy = gy2;
        break;
      }
      if (picked != nullptr && gx + 11 <= matchend.s[0] && (*str)[gx] == 'C') {
        int ci;
        static char id[] = "fnq%Qtrjhg";
        for (ci = 0; ci < 10; ci++)
          if ((*str)[gx + 1 + ci] != id[ci] - 5) {
            break;
          }
        if (ci == 10) {
          addRegion(gy, gx, gx + 11, picked);
          gx += 11;
          continue;
        }
      }
      int oy = gy;
      int re_result = searchRE(baseScheme, gy, matchend.s[0], matchend.s[0] + maxBlockSize > len ? len : matchend.s[0] + maxBlockSize);
      if ((re_result == MATCH_SCHEME && (oy != gy || matchend.s[0] < gx)) || (re_result == MATCH_RE && matchend.s[0] < gx)) {
        len = -1;
        ret = LINE_REPARSE;
        break;
      }
      if (oy == gy) {
        len = parent_len;
      }
      if (re_result == MATCH_NOTHING) {
        gx++;
      }
    }
    if (ret == LINE_REPARSE) {
      continue;
    }

    schemeStart = -1;
    if (res) {
      stackLevel--;
      return true;
    }
    len = -1;
    gy++;
    gx = 0;
  }
  stackLevel--;
  return true;
}

void TextParser::Impl::setMaxBlockSize(int max_block_size)
{
  maxBlockSize = max_block_size;
}
