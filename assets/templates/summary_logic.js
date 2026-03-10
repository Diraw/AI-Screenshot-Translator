if (document.body) { applyDarkMode(IS_DARK); } else { document.addEventListener('DOMContentLoaded', () => applyDarkMode(IS_DARK), {once:true}); }
var LAST_ENTRY = null;
document.addEventListener('DOMContentLoaded', function() {
    if (document.body) {
        document.body.tabIndex = -1;
        document.body.focus();
    }
    if (typeof RESTORE_SCROLL !== 'undefined') {
        window.scrollTo(0, RESTORE_SCROLL);
        setTimeout(() => window.scrollTo(0, RESTORE_SCROLL), 50);
    }
    log('DOMContentLoaded; KEY_VIEW='+KEY_VIEW+' KEY_SHOT='+KEY_SHOT+' KEY_EDIT='+KEY_EDIT);
});
function currentEntry() {
    var active = document.activeElement;
    if (active) {
        var e = active.closest('.entry');
        if (e) {
            LAST_ENTRY = e;
            return e;
        }
    }
    if (LAST_ENTRY) return LAST_ENTRY;
    return document.querySelector('.entry');
}
function updateStatus(mode) {
   var ind = document.getElementById('status-indicator');
   if (ind) {
       if (mode === 'edit') ind.innerText = 'MODE: EDIT';
       else if (mode === 'raw') ind.innerText = 'MODE: RAW';
       else ind.innerText = 'MODE: VIEW';
   }
}

function isSafeUrl(url, allowDataImage) {
   var v = (url || '').trim().toLowerCase();
   if (!v) return true;
   if (v.indexOf('javascript:') === 0 || v.indexOf('vbscript:') === 0) return false;
   if (v.indexOf('data:') === 0) return !!allowDataImage && v.indexOf('data:image/') === 0;
   return (
      v.indexOf('http://') === 0 ||
      v.indexOf('https://') === 0 ||
      v.indexOf('mailto:') === 0 ||
      v.indexOf('tel:') === 0 ||
      v.indexOf('#') === 0 ||
      v.indexOf('/') === 0 ||
      v.indexOf('./') === 0 ||
      v.indexOf('../') === 0
   );
}

function sanitizeRenderedHtml(html) {
   var template = document.createElement('template');
   template.innerHTML = html || '';

   var allowedTags = {
      A:1, B:1, BLOCKQUOTE:1, BR:1, CODE:1, DEL:1, DIV:1, EM:1,
      H1:1, H2:1, H3:1, H4:1, H5:1, H6:1, HR:1, I:1, IMG:1, LI:1,
      MARK:1, OL:1, P:1, PRE:1, S:1, SPAN:1, STRONG:1, SUB:1, SUP:1,
      TABLE:1, TBODY:1, TD:1, TH:1, THEAD:1, TR:1, U:1, UL:1
   };
   var blockedTags = { SCRIPT:1, STYLE:1, IFRAME:1, OBJECT:1, EMBED:1, LINK:1, META:1 };
   var globalAttrs = { 'class':1, 'title':1, 'aria-label':1, 'aria-hidden':1 };

   function clean(parent) {
      var node = parent.firstChild;
      while (node) {
         var next = node.nextSibling;

         if (node.nodeType === 1) {
            var tag = node.tagName.toUpperCase();
            if (blockedTags[tag]) {
               node.remove();
               node = next;
               continue;
            }
            if (!allowedTags[tag]) {
               while (node.firstChild) parent.insertBefore(node.firstChild, node);
               node.remove();
               node = next;
               continue;
            }

            var attrs = Array.prototype.slice.call(node.attributes || []);
            for (var i = 0; i < attrs.length; i++) {
               var name = attrs[i].name;
               var lower = name.toLowerCase();
               var value = attrs[i].value || '';
               var keep = !!globalAttrs[lower];

               if (tag === 'A' && (lower === 'href' || lower === 'target' || lower === 'rel')) keep = true;
               if (tag === 'IMG' && (lower === 'src' || lower === 'alt')) keep = true;
               if ((tag === 'CODE' || tag === 'PRE' || tag === 'SPAN' || tag === 'DIV' || tag === 'P') && lower === 'class') keep = true;

               if (lower.indexOf('on') === 0 || lower === 'style' || lower === 'srcdoc') keep = false;
               if ((lower === 'href' || lower === 'src') && keep) {
                  var allowDataImage = tag === 'IMG' && lower === 'src';
                  if (!isSafeUrl(value, allowDataImage)) keep = false;
               }

               if (!keep) node.removeAttribute(name);
            }

            if (tag === 'A') {
               var href = (node.getAttribute('href') || '').trim();
               if (href) node.setAttribute('rel', 'noopener noreferrer');
            }

            clean(node);
         } else if (node.nodeType === 8) {
            node.remove();
         }

         node = next;
      }
   }

   clean(template.content || template);
   return template.innerHTML;
}

function protectMathJs(text) {
   var blocks = [];
   var counter = 0;
   var regex = /(\$\$[\s\S]*?\$\$)|(\\\[[\s\S]*?\\\])|(\\\([\s\S]*?\\\))|(\$[^\$\n]+\$)/g;
   var protectedText = text.replace(regex, function(match) {
       var ph = 'MATHBLOCKPH' + counter++;
       blocks.push(match);
       return ph;
   });
   return {text: protectedText, blocks: blocks};
}

function extractMarkdown(rawEl) {
    if (!rawEl) return '';
    var html = rawEl.innerHTML || '';
    html = html.replace(/<div><br\s*\/?>\s*<\/div>/gi, '\n');
    html = html.replace(/<div>/gi, '\n').replace(/<\/div>/gi, '');
    html = html.replace(/<p>/gi, '').replace(/<\/p>/gi, '\n');
    html = html.replace(/<br\s*\/?>/gi, '\n');
    html = html.replace(/&nbsp;/gi, ' ');
    var tmp = document.createElement('div');
    tmp.style.whiteSpace = 'pre-wrap';
    tmp.innerHTML = html;
    return (tmp.textContent || '').replace(/\r\n?/g, '\n');
}

function ensureMarkedOptions() {
    if (typeof marked !== 'undefined') {
         marked.setOptions({ breaks: true, gfm: true });
    }
}

function renderContent(id, markdownOverride) {
    ensureMarkedOptions();
    var raw = document.getElementById('raw_' + id);
    var rendered = document.getElementById('rendered_' + id);
    var markdown = (typeof markdownOverride === 'string') ? markdownOverride : (raw ? raw.textContent : '');
    
    // Step 1: Extract LaTeX from backticks (e.g., `$formula$` -> $formula$)
    // Only unwrap if the backticked content is a valid LaTeX expression (properly paired delimiters)
    markdown = markdown.replace(/`([^`]+)`/g, function(match, inner) {
      // Check if content is a complete LaTeX expression with properly paired delimiters
      // Match: $...$, $$...$$, \(...\), or \[...\]
      // Using [\s\S]* to allow empty or any content including newlines
    if (/^(?:\$\$[\s\S]*?\$\$|\$[^\$\n]+\$|\\\([\s\S]*?\\\)|\\\[[\s\S]*?\\\])$/.test(inner.trim())) {
      return inner; // Unwrap backticks
    }
    return match; // Keep original backticks if not valid LaTeX
    });

    // Normalize common LaTeX display environments so KaTeX auto-render can process them
    // through the configured $$...$$ delimiters.
    markdown = markdown.replace(
      /\\begin\{(equation\*?|align\*?|gather\*?|multline\*?)\}([\s\S]*?)\\end\{\1\}/g,
      function(_match, _envName, body) {
        var inner = (body || '').trim();
        return '\n$$\n' + inner + '\n$$\n';
      }
    );
    
    // Step 2: Protect math expressions before markdown parsing
    var p = protectMathJs(markdown);
    
    // Step 3: Parse markdown
    var html = marked.parse(p.text);
    html = sanitizeRenderedHtml(html);
    
    // Step 4: Restore protected math blocks (reverse order to avoid prefix collisions, e.g., PH1 vs PH10)
    for (var r = p.blocks.length - 1; r >= 0; r--) {
         var block = p.blocks[r];
         try {
           var placeholder = 'MATHBLOCKPH' + r;
           var escapedPlaceholder = placeholder.replace(/[.*+?^${}()|[\\]\\]/g, '\\$&');
           var regex = new RegExp(escapedPlaceholder, 'g');
           html = html.replace(regex, function() { return block; });
         } catch (e) {
           console.error('Error restoring math block ' + r + ':', e);
         }
    }
    
    rendered.innerHTML = html;
    
    // Step 5: Render LaTeX with KaTeX
    requestAnimationFrame(function() {
      renderMathInElement(rendered, {
        delimiters: [
          {left: '$$', right: '$$', display: true}, 
          {left: '$', right: '$', display: false}, 
          {left: '\\(', right: '\\)', display: false}, 
          {left: '\\[', right: '\\]', display: true}
        ], 
        throwOnError: false
        // Use KaTeX defaults for ignoredTags/ignoredClasses for safety
      });
    });
}

function insertMarkdown(startTag, endTag) {
   var sel = window.getSelection();
   if (!sel.rangeCount) return;
   var range = sel.getRangeAt(0);
   var text = range.toString();
   document.execCommand('insertText', false, startTag + text + endTag);
}

function matchHotkey(e, hotkeyStr) {
   if (!hotkeyStr) return false;
   var parts = hotkeyStr.toLowerCase().split('+').map(function(p){ return p.trim(); }).filter(Boolean);
   if (!parts.length) return false;
   var key = parts.pop();
   var ctrl = parts.includes('ctrl') || parts.includes('control');
   var alt = parts.includes('alt');
   var shift = parts.includes('shift');
   return (e.key.toLowerCase() === key && e.ctrlKey === ctrl && e.altKey === alt && e.shiftKey === shift);
}

function toggleView(id) {
  var raw = document.getElementById('raw_' + id);
  var rendered = document.getElementById('rendered_' + id);
  if (raw.style.display === 'none') {
    raw.style.display = 'block';
    rendered.style.display = 'none';
    updateStatus('raw');
  } else {
    raw.style.display = 'none';
    rendered.style.display = 'block';
    updateStatus('view');
  }
}

function toggleEdit(entry) {
   var id = entry.getAttribute('data-id');
   var rendered = document.getElementById('rendered_' + id);
   var raw = document.getElementById('raw_' + id);
   
   if (entry.classList.contains('mode-edit')) {
       entry.classList.remove('mode-edit');
       entry.classList.add('mode-view');
       updateStatus('view');
       raw.contentEditable = 'false';

       var markdown = extractMarkdown(raw);
       raw.innerText = markdown;
       renderContent(id, markdown);

       raw.style.display = 'none';
       rendered.style.display = 'block';
       
       // Notify C++ of edit
       window.cmd_updateEntry(id, markdown);

       entry.focus();
   } else {
       entry.classList.remove('mode-view');
       entry.classList.add('mode-edit');
       updateStatus('edit');
       rendered.style.display = 'none';
       raw.style.display = 'block';
       raw.contentEditable = 'true';
       raw.focus();
   }
}

document.addEventListener('focusin', function(e) {
   var entry = e.target.closest('.entry');
   if (entry) {
       var id = entry.getAttribute('data-id');
       var raw = document.getElementById('raw_' + id);
       var isRaw = raw && raw.style.display !== 'none';
       if (entry.classList.contains('mode-edit')) updateStatus('edit');
       else if (isRaw) updateStatus('raw');
       else updateStatus('view');
       LAST_ENTRY = entry;
   } else { updateStatus('view'); }
});

function handleKey(e) {
   log(`handleKey enter key=${e.key} ctrl=${e.ctrlKey} alt=${e.altKey} shift=${e.shiftKey}`);
   var keyLower = e.key ? e.key.toLowerCase() : '';
   if (keyLower === 'f12' || (e.ctrlKey && e.shiftKey && keyLower === 'i')) {
       if (window.cmd_openDevTools) {
           log('Trigger openDevTools from JS');
           window.cmd_openDevTools();
       } else {
           log('cmd_openDevTools not bound');
       }
       e.preventDefault();
       return;
   }
   if (SELECTION_MODE && e.key === 'Escape') {
       if (window.cmd_exitSelectionMode) window.cmd_exitSelectionMode();
       e.preventDefault();
       return;
   }
   var entry = currentEntry();
   if (!entry) { log('handleKey: no entry'); return; }
   log(`handleKey currentEntry id=${entry.getAttribute('data-id')}`);
   
   var isEditing = entry.classList.contains('mode-edit');
   var k = keyLower;
   log(`KEY_VIEW=${KEY_VIEW} KEY_SHOT=${KEY_SHOT} k=${k} isEditing=${isEditing}`);

   // Batch selection mode toggle (only when not editing).
   // Default is Ctrl+S, but it's configurable via KEY_SELECT.
   if (!isEditing && matchHotkey(e, (typeof KEY_SELECT !== 'undefined') ? KEY_SELECT : 'ctrl+s')) {
       if (window.cmd_toggleSelectionMode) window.cmd_toggleSelectionMode();
       e.preventDefault();
       return;
   }

   if (matchHotkey(e, KEY_EDIT)) {
       e.preventDefault(); toggleEdit(entry); return; 
   }
   
   if (isEditing) {
      if (e.key === 'Escape') {
          e.preventDefault(); toggleEdit(entry); return;
      }
      if (matchHotkey(e, KEY_BOLD)) {
          e.preventDefault(); insertMarkdown('**', '**'); return;
      }
      if (matchHotkey(e, KEY_UNDERLINE)) {
          e.preventDefault(); insertMarkdown('<u>', '</u>'); return;
      }
      if (matchHotkey(e, KEY_HIGHLIGHT)) {
          e.preventDefault(); insertMarkdown('<mark>', '</mark>'); return;
      }
      return; 
    }
    if (!isEditing) {
        // Esc exits RAW mode back to VIEW (when currently in RAW)
        if (e.key === 'Escape') {
            var id0 = entry.getAttribute('data-id');
            var raw0 = document.getElementById('raw_' + id0);
            if (raw0 && raw0.style.display !== 'none') {
                e.preventDefault();
                toggleView(id0);
                return;
            }
        }
        if (matchHotkey(e, KEY_VIEW)) { toggleView(entry.getAttribute('data-id')); e.preventDefault(); return; }
        if (matchHotkey(e, KEY_SHOT)) { 
            // Call native webview binding
            window.cmd_restore(entry.getAttribute('data-id'));
            e.preventDefault(); return; 
        }
        
        // Handle 'dd' for deletion
        if (k === 'd') {
            e.preventDefault();
            var now = Date.now();
            if (entry.lastDTime && (now - entry.lastDTime < 500)) {
                 // Double press detected
                 var id = entry.getAttribute('data-id');
                 // No confirm dialog as requested
                 window.cmd_delete(id);
                 entry.remove();
                 entry.lastDTime = 0;
            } else {
                 entry.lastDTime = now;
            }
            return;
        }
    }
}

if (!window.__INIT_ONCE__) {
    window.__INIT_ONCE__ = true;
    window.__FOCUS_GUARD_ID__ = (Math.random().toString(16).slice(2) + Date.now().toString(16));
    log('init_guard gid=' + window.__FOCUS_GUARD_ID__);
    window.addEventListener('keydown', handleKey, true);
    window.addEventListener('keydown', function(e){
        log(`keydown key=${e.key} ctrl=${e.ctrlKey} alt=${e.altKey} shift=${e.shiftKey} target=${(e.target && e.target.tagName)}`);
    }, true);
    // 非 capture：避免把元素级 focus/blur 误记为 window focus/blur。
    window.addEventListener('focus', ()=>log('window focus gid=' + window.__FOCUS_GUARD_ID__));
    window.addEventListener('blur', ()=>log('window blur gid=' + window.__FOCUS_GUARD_ID__));
    document.addEventListener('visibilitychange', ()=>log('visibility='+document.visibilityState), true);
}

function updateEntryInDom(id, newMarkdown) {
   var raw = document.getElementById('raw_' + id);
   if (raw) {
       raw.innerText = newMarkdown;
       var entry = raw.closest('.entry');
       if (entry && !entry.classList.contains('mode-edit')) {
           renderContent(id, newMarkdown);
       }
   }
}

function addEntryToDom(id, time, markdown, mathBlocks, originalRaw, isSelectionMode, tags) {
  var div = document.createElement('div');
  div.className = 'entry mode-view';
  div.id = 'entry_' + id;
  div.setAttribute('data-id', id);
  div.tabIndex = 0;

  var checkboxDisplay = isSelectionMode ? 'block' : 'none';

  var header = document.createElement('div');
  header.className = 'entry-header';

  var checkbox = document.createElement('input');
  checkbox.type = 'checkbox';
  checkbox.className = 'selection-checkbox';
  checkbox.style.display = checkboxDisplay;
  checkbox.setAttribute('data-id', id);
  header.appendChild(checkbox);

  var info = document.createElement('div');
  info.className = 'entry-info';

  var timeLine = document.createElement('div');
  timeLine.textContent = time;
  info.appendChild(timeLine);

  if (tags.length) {
      var tagLine = document.createElement('div');
      tagLine.textContent = 'Tags: ' + tags.join(', ');
      info.appendChild(tagLine);
  }

  header.appendChild(info);
  div.appendChild(header);

  var contentArea = document.createElement('div');
  contentArea.className = 'content-area';

  var rendered = document.createElement('div');
  rendered.id = 'rendered_' + id;
  rendered.className = 'rendered-html';
  contentArea.appendChild(rendered);

  var raw = document.createElement('div');
  raw.id = 'raw_' + id;
  raw.className = 'raw-text';
  raw.style.display = 'none';
  raw.spellcheck = false;
  contentArea.appendChild(raw);

  div.appendChild(contentArea);
  document.body.appendChild(div);
  
  var rawContainer = document.getElementById('raw_' + id);
  if (rawContainer) {
      rawContainer.textContent = originalRaw; 
      renderContent(id);
  }
}

function addEntryFromNative(entry, isSelectionMode) {
    if (!entry || typeof entry !== 'object') return;
    var tags = Array.isArray(entry.tags) ? entry.tags : [];
    addEntryToDom(
        String(entry.id || ''),
        String(entry.time || ''),
        '',
        [],
        String(entry.originalRaw || ''),
        !!isSelectionMode,
        tags
    );
}

function bootstrapEntriesFromNative(scriptId, isSelectionMode) {
    var dataNode = document.getElementById(scriptId);
    if (!dataNode) return;
    var raw = dataNode.textContent || '[]';
    var entries = [];
    try {
        entries = JSON.parse(raw);
    } catch (e) {
        console.error('Failed to parse initial entry JSON:', e);
        return;
    }
    if (!Array.isArray(entries)) return;
    SELECTION_MODE = !!isSelectionMode;
    entries.forEach(function(entry) {
        addEntryFromNative(entry, isSelectionMode);
    });
}

function toggleSelectionMode(show) {
    SELECTION_MODE = !!show;
    var checkboxes = document.querySelectorAll('.selection-checkbox');
    checkboxes.forEach(cb => {
        cb.style.display = show ? 'block' : 'none';
        if (!show) {
            cb.checked = false;
            var entry = cb.closest('.entry');
            if (entry) entry.classList.remove('selected');
        }
    });
}

function getSelectedIds() {
    var checkboxes = document.querySelectorAll('.selection-checkbox:checked');
    return Array.from(checkboxes).map(cb => cb.getAttribute('data-id'));
}

function selectAllEntries(select) {
    var checkboxes = document.querySelectorAll('.selection-checkbox');
    checkboxes.forEach(cb => {
        cb.checked = select;
        var entry = cb.closest('.entry');
        if (entry) entry.classList.toggle('selected', select);
    });
}

// Click-to-select in selection mode
document.addEventListener('click', function(e) {
    if (!SELECTION_MODE) return;
    var entry = e.target.closest('.entry');
    if (!entry) return;
    var cb = entry.querySelector('.selection-checkbox');
    if (!cb) return;
    cb.checked = !cb.checked;
    entry.classList.toggle('selected', cb.checked);
    e.preventDefault();
});

// Drag rectangle selection
let dragSelect = false;
let dragStart = {x:0, y:0};
let selectionRectEl = null;
document.addEventListener('mousedown', function(e){
    if (!SELECTION_MODE || e.button !== 0) return;
    dragSelect = true;
    dragStart = {x: e.clientX, y: e.clientY};
    selectionRectEl = document.createElement('div');
    selectionRectEl.className = 'selection-rect';
    document.body.appendChild(selectionRectEl);
});
document.addEventListener('mousemove', function(e){
    if (!dragSelect || !selectionRectEl) return;
    var x1 = Math.min(dragStart.x, e.clientX);
    var y1 = Math.min(dragStart.y, e.clientY);
    var x2 = Math.max(dragStart.x, e.clientX);
    var y2 = Math.max(dragStart.y, e.clientY);
    selectionRectEl.style.left = x1 + 'px';
    selectionRectEl.style.top = y1 + 'px';
    selectionRectEl.style.width = (x2 - x1) + 'px';
    selectionRectEl.style.height = (y2 - y1) + 'px';
    var checkboxes = document.querySelectorAll('.selection-checkbox');
    checkboxes.forEach(cb => {
        var entry = cb.closest('.entry');
        if (!entry) return;
        var rect = entry.getBoundingClientRect();
        var overlap = rect.left < x2 && rect.right > x1 && rect.top < y2 && rect.bottom > y1;
        cb.checked = overlap;
        entry.classList.toggle('selected', cb.checked);
    });
});
document.addEventListener('mouseup', function(e){
    if (!dragSelect) return;
    dragSelect = false;
    if (selectionRectEl) {
        selectionRectEl.remove();
        selectionRectEl = null;
    }
});
