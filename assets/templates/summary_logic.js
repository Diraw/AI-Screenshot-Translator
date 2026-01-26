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
    var p = protectMathJs(markdown);
    var html = marked.parse(p.text);
    p.blocks.forEach(function(block, index) {
         html = html.replace('MATHBLOCKPH' + index, block);
    });
    rendered.innerHTML = html;
    requestAnimationFrame(function() {
      renderMathInElement(rendered, {delimiters: [{left: '$$', right: '$$', display: true}, {left: '$', right: '$', display: false}, {left: '\\(', right: '\\)', display: false}, {left: '\\[', right: '\\]', display: true}], throwOnError : false});
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

  var tagText = tags.length ? `Tags: ${tags.join(', ')}` : '';

  div.innerHTML = `
    <div class='entry-header'>
        <input type='checkbox' class='selection-checkbox' style='display: ${checkboxDisplay}' data-id='${id}'>
        <div class='entry-info'>
            <div>${time}</div>
            ${tagText ? `<div>${tagText}</div>` : ''}
        </div>
    </div>
    <div class='content-area'>
        <div id='rendered_${id}' class='rendered-html'></div>
        <div id='raw_${id}' class='raw-text' style='display:none;' spellcheck='false'></div>
    </div>
  `;
  document.body.appendChild(div);
  
  var rawContainer = document.getElementById('raw_' + id);
  if (rawContainer) {
      rawContainer.textContent = originalRaw; 
      renderContent(id);
  }
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
 </script>
