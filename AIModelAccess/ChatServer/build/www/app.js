(function () {
  const MAX_MESSAGE_LENGTH = 2000;
  const state = {
    sessions: [],
    models: [],
    activeSessionId: "",
    activeSessionModel: "",
    selectedModelName: "",
    isStreaming: false
  };

  const elements = {
    sessionList: document.getElementById("sessionList"),
    sessionCount: document.getElementById("sessionCount"),
    welcomeState: document.getElementById("welcomeState"),
    chatWorkspace: document.getElementById("chatWorkspace"),
    chatTitle: document.getElementById("chatTitle"),
    chatSubtitle: document.getElementById("chatSubtitle"),
    messageList: document.getElementById("messageList"),
    messageInput: document.getElementById("messageInput"),
    sendButton: document.getElementById("sendButton"),
    charCount: document.getElementById("charCount"),
    streamingBadge: document.getElementById("streamingBadge"),
    modelModal: document.getElementById("modelModal"),
    modelList: document.getElementById("modelList"),
    closeModalBtn: document.getElementById("closeModalBtn"),
    confirmCreateSessionBtn: document.getElementById("confirmCreateSessionBtn"),
    cancelCreateSessionBtn: document.getElementById("cancelCreateSessionBtn"),
    headerNewChatBtn: document.getElementById("headerNewChatBtn"),
    emptyNewChatBtn: document.getElementById("emptyNewChatBtn"),
    sessionItemTemplate: document.getElementById("sessionItemTemplate"),
    messageTemplate: document.getElementById("messageTemplate")
  };

  document.addEventListener("DOMContentLoaded", init);

  async function init() {
    bindEvents();
    updateCharCount();
    renderSessionListLoading();
    await loadSessions();
  }

  function bindEvents() {
    elements.headerNewChatBtn.addEventListener("click", openModelModal);
    elements.emptyNewChatBtn.addEventListener("click", openModelModal);
    elements.closeModalBtn.addEventListener("click", closeModelModal);
    elements.cancelCreateSessionBtn.addEventListener("click", closeModelModal);
    elements.confirmCreateSessionBtn.addEventListener("click", createSessionFromSelection);
    elements.sendButton.addEventListener("click", sendCurrentMessage);
    elements.messageInput.addEventListener("input", handleComposerInput);
    elements.messageInput.addEventListener("keydown", handleComposerKeydown);
    elements.modelModal.addEventListener("click", (event) => {
      if (event.target.classList.contains("modal-backdrop")) {
        closeModelModal();
      }
    });
    elements.messageList.addEventListener("click", handleMessageListClick);
  }

  async function requestJson(url, options) {
    const response = await fetch(url, {
      headers: {
        "Content-Type": "application/json"
      },
      ...options
    });

    const payload = await response.json().catch(() => ({
      success: false,
      message: "服务器返回了无法解析的数据。"
    }));

    if (!response.ok || payload.success === false) {
      throw new Error(payload.message || "请求失败");
    }

    return payload;
  }

  async function loadSessions() {
    try {
      const payload = await requestJson("/api/sessions");
      state.sessions = Array.isArray(payload.data) ? payload.data : [];
      renderSessionList();

      if (state.activeSessionId) {
        const activeExists = state.sessions.some((session) => session.id === state.activeSessionId);
        if (activeExists) {
          return;
        }
      }

      if (state.sessions.length === 0) {
        showWelcomeState();
      }
    } catch (error) {
      renderSessionListError(error.message || "获取会话列表失败");
    }
  }

  function renderSessionListLoading() {
    elements.sessionList.innerHTML = '<div class="loading-state">会话列表加载中...</div>';
  }

  function renderSessionListError(message) {
    elements.sessionCount.textContent = "0 条";
    elements.sessionList.innerHTML = `<div class="error-state">${escapeHtml(message)}</div>`;
  }

  function renderSessionList() {
    elements.sessionCount.textContent = `${state.sessions.length} 条`;
    elements.sessionList.innerHTML = "";

    if (state.sessions.length === 0) {
      elements.sessionList.innerHTML = '<div class="empty-list">还没有历史会话，点击“新建对话”开始吧。</div>';
      return;
    }

    const fragment = document.createDocumentFragment();
    for (const session of state.sessions) {
      const node = elements.sessionItemTemplate.content.firstElementChild.cloneNode(true);
      node.dataset.sessionId = session.id;
      node.classList.toggle("active", session.id === state.activeSessionId);
      node.querySelector(".session-updated-at").textContent = formatDateTime(session.updated_at);
      node.querySelector(".session-preview").textContent = session.first_user_message || "新会话，等待第一条消息";
      node.querySelector(".session-model").textContent = session.model || "未知模型";
      node.querySelector(".session-message-count").textContent = `${session.message_count || 0} 条消息`;

      const deleteBtn = node.querySelector(".session-delete-btn");
      deleteBtn.dataset.sessionId = session.id;
      deleteBtn.addEventListener("click", (event) => {
        event.stopPropagation();
        deleteSession(session.id);
      });

      node.addEventListener("click", () => {
        switchSession(session.id);
      });
      node.addEventListener("keydown", (event) => {
        if (event.key === "Enter" || event.key === " ") {
          event.preventDefault();
          switchSession(session.id);
        }
      });
      fragment.appendChild(node);
    }
    elements.sessionList.appendChild(fragment);
  }

  async function openModelModal() {
    try {
      if (state.models.length === 0) {
        elements.modelList.innerHTML = '<div class="loading-state">模型列表加载中...</div>';
      }
      elements.modelModal.classList.remove("hidden");
      const payload = await requestJson("/api/models");
      state.models = Array.isArray(payload.data) ? payload.data : [];
      state.selectedModelName = state.models[0] ? state.models[0].name : "";
      renderModelList();
    } catch (error) {
      elements.modelList.innerHTML = `<div class="error-state">${escapeHtml(error.message || "获取模型失败")}</div>`;
    }
  }

  function closeModelModal() {
    elements.modelModal.classList.add("hidden");
  }

  function renderModelList() {
    elements.modelList.innerHTML = "";
    if (state.models.length === 0) {
      elements.modelList.innerHTML = '<div class="empty-list">当前没有可用模型。</div>';
      return;
    }

    const fragment = document.createDocumentFragment();
    for (const model of state.models) {
      const label = document.createElement("label");
      label.className = "model-card";
      label.classList.toggle("selected", model.name === state.selectedModelName);

      const radio = document.createElement("input");
      radio.type = "radio";
      radio.name = "session-model";
      radio.value = model.name;
      radio.checked = model.name === state.selectedModelName;
      radio.addEventListener("change", () => {
        state.selectedModelName = model.name;
        renderModelList();
      });

      const title = document.createElement("h4");
      title.className = "model-card-title";
      title.textContent = model.name || "未命名模型";

      const desc = document.createElement("p");
      desc.className = "model-card-desc";
      desc.textContent = model.desc || "暂无模型描述";

      label.appendChild(radio);
      label.appendChild(title);
      label.appendChild(desc);
      fragment.appendChild(label);
    }
    elements.modelList.appendChild(fragment);
  }

  async function createSessionFromSelection() {
    if (!state.selectedModelName) {
      window.alert("请先选择一个模型。");
      return;
    }

    elements.confirmCreateSessionBtn.disabled = true;
    try {
      const payload = await requestJson("/api/session", {
        method: "POST",
        body: JSON.stringify({
          model: state.selectedModelName
        })
      });

      closeModelModal();
      state.activeSessionId = payload.data.session_id;
      state.activeSessionModel = payload.data.model;
      showChatWorkspace();
      elements.chatTitle.textContent = "新会话";
      elements.chatSubtitle.textContent = `当前模型：${state.activeSessionModel}`;
      elements.messageList.innerHTML = "";
      await loadSessions();
      await switchSession(state.activeSessionId, true);
    } catch (error) {
      window.alert(error.message || "创建会话失败");
    } finally {
      elements.confirmCreateSessionBtn.disabled = false;
    }
  }

  async function switchSession(sessionId, skipSessionRefresh) {
    if (!sessionId) {
      return;
    }

    state.activeSessionId = sessionId;
    const session = state.sessions.find((item) => item.id === sessionId);
    if (session) {
      state.activeSessionModel = session.model || "";
      elements.chatTitle.textContent = session.first_user_message || "未命名对话";
      elements.chatSubtitle.textContent = `当前模型：${session.model || "未知模型"} · 更新时间 ${formatDateTime(session.updated_at)}`;
    }

    showChatWorkspace();
    renderSessionList();
    renderMessagesLoading();

    try {
      const payload = await requestJson(`/api/session/${encodeURIComponent(sessionId)}/history`);
      renderMessageList(Array.isArray(payload.data) ? payload.data : []);
      if (!skipSessionRefresh) {
        elements.messageInput.focus();
      }
    } catch (error) {
      elements.messageList.innerHTML = `<div class="error-state">${escapeHtml(error.message || "获取历史消息失败")}</div>`;
    }
  }

  async function deleteSession(sessionId) {
    if (!sessionId) {
      return;
    }

    const confirmed = window.confirm("确认删除这个会话吗？");
    if (!confirmed) {
      return;
    }

    try {
      await requestJson(`/api/session/${encodeURIComponent(sessionId)}`, {
        method: "DELETE"
      });

      if (state.activeSessionId === sessionId) {
        state.activeSessionId = "";
        state.activeSessionModel = "";
        showWelcomeState();
      }
      await loadSessions();
    } catch (error) {
      window.alert(error.message || "删除会话失败");
    }
  }

  function showWelcomeState() {
    elements.welcomeState.classList.remove("hidden");
    elements.chatWorkspace.classList.add("hidden");
    elements.messageList.innerHTML = "";
  }

  function showChatWorkspace() {
    elements.welcomeState.classList.add("hidden");
    elements.chatWorkspace.classList.remove("hidden");
  }

  function renderMessagesLoading() {
    elements.messageList.innerHTML = '<div class="loading-state">消息加载中...</div>';
  }

  function renderMessageList(messages) {
    elements.messageList.innerHTML = "";
    if (!messages.length) {
      elements.messageList.innerHTML = '<div class="empty-list">会话已创建，发送第一条消息开始聊天吧。</div>';
      return;
    }

    const fragment = document.createDocumentFragment();
    for (const message of messages) {
      fragment.appendChild(createMessageNode(message.role, message.content, message.timestamp));
    }
    elements.messageList.appendChild(fragment);
    scrollMessagesToBottom();
  }

  function createMessageNode(role, content, timestamp) {
    const node = elements.messageTemplate.content.firstElementChild.cloneNode(true);
    const normalizedRole = role === "assistant" ? "assistant" : "user";
    node.classList.add(normalizedRole);
    node.querySelector(".message-role").textContent = normalizedRole === "assistant" ? "AI 回复" : "我";
    node.querySelector(".message-content").innerHTML = renderMarkdown(content || "");
    node.querySelector(".message-time").textContent = formatDateTime(timestamp);
    return node;
  }

  async function sendCurrentMessage() {
    const text = elements.messageInput.value.trim();
    if (!state.activeSessionId) {
      window.alert("请先新建一个会话。");
      return;
    }
    if (!text) {
      return;
    }
    if (state.isStreaming) {
      return;
    }

    state.isStreaming = true;
    setStreamingState(true);

    const now = Math.floor(Date.now() / 1000);
    clearEmptyStateIfNeeded();
    elements.messageList.appendChild(createMessageNode("user", text, now));

    const assistantNode = createMessageNode("assistant", "", now);
    assistantNode.querySelector(".message-time").textContent = "生成中...";
    elements.messageList.appendChild(assistantNode);
    scrollMessagesToBottom();

    elements.messageInput.value = "";
    resizeTextarea();
    updateCharCount();

    try {
      const response = await fetch("/api/message/async", {
        method: "POST",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify({
          session_id: state.activeSessionId,
          message: text
        })
      });

      if (!response.ok || !response.body) {
        throw new Error("发送消息失败");
      }

      const contentElement = assistantNode.querySelector(".message-content");
      let fullText = "";
      await consumeSseStream(response.body, (chunk) => {
        fullText += chunk;
        contentElement.innerHTML = renderMarkdown(fullText);
        assistantNode.querySelector(".message-time").textContent = "生成中...";
        scrollMessagesToBottom();
      });

      assistantNode.querySelector(".message-time").textContent = formatDateTime(Math.floor(Date.now() / 1000));
      await loadSessions();
      await switchSession(state.activeSessionId, true);
    } catch (error) {
      assistantNode.querySelector(".message-content").innerHTML = `<p>${escapeHtml(error.message || "消息发送失败")}</p>`;
      assistantNode.querySelector(".message-time").textContent = formatDateTime(Math.floor(Date.now() / 1000));
    } finally {
      state.isStreaming = false;
      setStreamingState(false);
      elements.messageInput.focus();
    }
  }

  async function consumeSseStream(stream, onChunk) {
    const reader = stream.getReader();
    const decoder = new TextDecoder("utf-8");
    let buffer = "";

    while (true) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }

      buffer += decoder.decode(value, { stream: true });
      const events = buffer.split("\n\n");
      buffer = events.pop() || "";

      for (const eventChunk of events) {
        const dataLines = eventChunk
          .split("\n")
          .filter((line) => line.startsWith("data:"))
          .map((line) => line.slice(5).trim());

        if (!dataLines.length) {
          continue;
        }

        const data = dataLines.join("\n");
        if (!data || data === '""') {
          continue;
        }
        if (data === "[DONE]") {
          return;
        }
        if (data.startsWith("[ERROR]")) {
          throw new Error(data.replace("[ERROR]", "").trim() || "流式回复失败");
        }

        let text = data;
        if ((data.startsWith('"') && data.endsWith('"')) || (data.startsWith("'") && data.endsWith("'"))) {
          try {
            text = JSON.parse(data);
          } catch (error) {
            text = data;
          }
        }
        onChunk(text);
      }
    }
  }

  function setStreamingState(streaming) {
    elements.sendButton.disabled = streaming;
    elements.headerNewChatBtn.disabled = streaming;
    elements.emptyNewChatBtn.disabled = streaming;
    elements.streamingBadge.classList.toggle("hidden", !streaming);
  }

  function clearEmptyStateIfNeeded() {
    const stateBox = elements.messageList.querySelector(".empty-list, .loading-state, .error-state");
    if (stateBox) {
      elements.messageList.innerHTML = "";
    }
  }

  function scrollMessagesToBottom() {
    elements.messageList.scrollTop = elements.messageList.scrollHeight;
  }

  function handleComposerInput() {
    updateCharCount();
    resizeTextarea();
  }

  function handleComposerKeydown(event) {
    if (event.key === "Enter" && !event.shiftKey) {
      event.preventDefault();
      sendCurrentMessage();
    }
  }

  function resizeTextarea() {
    elements.messageInput.style.height = "auto";
    elements.messageInput.style.height = `${Math.min(elements.messageInput.scrollHeight, 180)}px`;
  }

  function updateCharCount() {
    const length = elements.messageInput.value.length;
    elements.charCount.textContent = `${length}/${MAX_MESSAGE_LENGTH}`;
  }

  function handleMessageListClick(event) {
    const button = event.target.closest(".copy-code-btn");
    if (!button) {
      return;
    }

    const code = button.closest(".code-block")?.querySelector("code");
    if (!code) {
      return;
    }

    const text = code.dataset.raw || code.textContent || "";
    copyText(text).then(() => {
      const original = button.textContent;
      button.textContent = "已复制";
      button.classList.add("copied");
      window.setTimeout(() => {
        button.textContent = original;
        button.classList.remove("copied");
      }, 1200);
    }).catch(() => {
      window.alert("复制失败，请手动复制代码。");
    });
  }

  function formatDateTime(timestamp) {
    const time = Number(timestamp);
    if (!time) {
      return "刚刚";
    }
    const now = Math.floor(Date.now() / 1000);
    const diffSeconds = now - time;
    if (diffSeconds >= 0 && diffSeconds <= 3600) {
      if (diffSeconds < 60) {
        return "刚刚";
      }
      const diffMinutes = Math.floor(diffSeconds / 60);
      if (diffMinutes < 60) {
        return `${diffMinutes}分钟前`;
      }
      return "1小时前";
    }
    return new Date(time * 1000).toLocaleString("zh-CN", {
      year: "numeric",
      month: "2-digit",
      day: "2-digit",
      hour: "2-digit",
      minute: "2-digit"
    });
  }

  function renderMarkdown(markdown) {
    const text = String(markdown || "").replace(/\r\n/g, "\n");
    const blocks = [];
    const codePattern = /```([\w#+.-]*)\n([\s\S]*?)```/g;
    let lastIndex = 0;
    let match;

    while ((match = codePattern.exec(text)) !== null) {
      if (match.index > lastIndex) {
        blocks.push(renderMarkdownText(text.slice(lastIndex, match.index)));
      }
      blocks.push(renderCodeBlock(match[2], match[1] || ""));
      lastIndex = match.index + match[0].length;
    }

    if (lastIndex < text.length) {
      blocks.push(renderMarkdownText(text.slice(lastIndex)));
    }

    return blocks.join("") || "<p></p>";
  }

  function renderMarkdownText(input) {
    const lines = input.split("\n");
    const html = [];
    let listType = "";

    const closeListIfNeeded = () => {
      if (listType) {
        html.push(`</${listType}>`);
        listType = "";
      }
    };

    for (let index = 0; index < lines.length; index += 1) {
      const line = lines[index];
      const trimmed = line.trim();
      if (!trimmed) {
        closeListIfNeeded();
        continue;
      }

      if (isMarkdownTableStart(lines, index)) {
        closeListIfNeeded();
        const tableLines = [lines[index], lines[index + 1]];
        index += 2;
        while (index < lines.length && lines[index].trim().includes("|")) {
          tableLines.push(lines[index]);
          index += 1;
        }
        index -= 1;
        html.push(renderTableBlock(tableLines));
        continue;
      }

      const headingMatch = trimmed.match(/^(#{1,4})\s+(.*)$/);
      if (headingMatch) {
        closeListIfNeeded();
        const level = Math.min(headingMatch[1].length, 4);
        html.push(`<h${level}>${renderInlineMarkdown(headingMatch[2])}</h${level}>`);
        continue;
      }

      const quoteMatch = trimmed.match(/^>\s?(.*)$/);
      if (quoteMatch) {
        closeListIfNeeded();
        html.push(`<blockquote>${renderInlineMarkdown(quoteMatch[1])}</blockquote>`);
        continue;
      }

      const orderedMatch = trimmed.match(/^\d+\.\s+(.*)$/);
      if (orderedMatch) {
        if (listType !== "ol") {
          closeListIfNeeded();
          listType = "ol";
          html.push("<ol>");
        }
        html.push(`<li>${renderInlineMarkdown(orderedMatch[1])}</li>`);
        continue;
      }

      const unorderedMatch = trimmed.match(/^[-*]\s+(.*)$/);
      if (unorderedMatch) {
        if (listType !== "ul") {
          closeListIfNeeded();
          listType = "ul";
          html.push("<ul>");
        }
        html.push(`<li>${renderInlineMarkdown(unorderedMatch[1])}</li>`);
        continue;
      }

      closeListIfNeeded();
      html.push(`<p>${renderInlineMarkdown(trimmed)}</p>`);
    }

    closeListIfNeeded();
    return html.join("");
  }

  function renderInlineMarkdown(text) {
    let html = escapeHtml(text);
    html = html.replace(/`([^`]+)`/g, (_, code) => `<code class="inline-code">${escapeHtml(code)}</code>`);
    html = html.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
    html = html.replace(/\*([^*]+)\*/g, "<em>$1</em>");
    html = html.replace(/\[([^\]]+)\]\((https?:\/\/[^)]+)\)/g, '<a href="$2" target="_blank" rel="noopener noreferrer">$1</a>');
    return html;
  }

  function renderCodeBlock(code, language) {
    const normalizedLanguage = (language || "text").trim().toLowerCase();
    const raw = code.replace(/\n$/, "");
    const highlighted = highlightCode(raw, normalizedLanguage);
    return [
      '<section class="code-block">',
      '<div class="code-toolbar">',
      `<span>${escapeHtml(normalizedLanguage || "text")}</span>`,
      '<button class="copy-code-btn" type="button">复制代码</button>',
      "</div>",
      `<pre><code data-raw="${escapeAttribute(raw)}">${highlighted}</code></pre>`,
      "</section>"
    ].join("");
  }

  function renderTableBlock(lines) {
    const headerCells = splitTableCells(lines[0]);
    const bodyLines = lines.slice(2).filter((line) => line.trim());
    const thead = `<thead><tr>${headerCells.map((cell) => `<th>${renderInlineMarkdown(cell)}</th>`).join("")}</tr></thead>`;
    const tbodyRows = bodyLines.map((line) => {
      const cells = splitTableCells(line);
      return `<tr>${cells.map((cell) => `<td>${renderInlineMarkdown(cell)}</td>`).join("")}</tr>`;
    }).join("");
    return [
      '<section class="table-block">',
      '<div class="table-scroll">',
      `<table>${thead}<tbody>${tbodyRows}</tbody></table>`,
      "</div>",
      "</section>"
    ].join("");
  }

  function isMarkdownTableStart(lines, index) {
    if (index + 1 >= lines.length) {
      return false;
    }
    const headerLine = lines[index].trim();
    const separatorLine = lines[index + 1].trim();
    if (!headerLine.includes("|") || !separatorLine.includes("|")) {
      return false;
    }
    const cells = splitTableCells(headerLine);
    if (cells.length < 2) {
      return false;
    }
    const separators = splitTableCells(separatorLine);
    if (separators.length !== cells.length) {
      return false;
    }
    return separators.every((cell) => /^:?-{3,}:?$/.test(cell.replace(/\s+/g, "")));
  }

  function splitTableCells(line) {
    return line
      .trim()
      .replace(/^\|/, "")
      .replace(/\|$/, "")
      .split("|")
      .map((cell) => cell.trim());
  }

  function highlightCode(code, language) {
    const tokenPattern = /\/\/.*$|#.*$|"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|\b(?:function|return|const|let|var|if|else|for|while|switch|case|break|continue|class|new|try|catch|finally|throw|async|await|import|export|from|default|public|private|protected|static|void|int|float|double|bool|char|string|struct|include|namespace|using|auto|template|typename)\b|\b\d+(?:\.\d+)?\b|\b[A-Za-z_]\w*(?=\s*\()|(?:==|=>|<=|>=|&&|\|\||\+|-|\*|\/|=)/gm;
    let html = "";
    let lastIndex = 0;
    let match;

    while ((match = tokenPattern.exec(code)) !== null) {
      const token = match[0];
      html += escapeHtml(code.slice(lastIndex, match.index));
      html += wrapHighlightedToken(token, language);
      lastIndex = match.index + token.length;
    }

    html += escapeHtml(code.slice(lastIndex));
    return html;
  }

  function wrapHighlightedToken(token, language) {
    if (/^\/\/.*$/.test(token)) {
      return `<span class="token-comment">${escapeHtml(token)}</span>`;
    }
    if (/^#.*$/.test(token) && ["python", "shell", "bash", "yaml", "yml"].includes(language)) {
      return `<span class="token-comment">${escapeHtml(token)}</span>`;
    }
    if (/^"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'$/.test(token)) {
      return `<span class="token-string">${escapeHtml(token)}</span>`;
    }
    if (/^\d+(?:\.\d+)?$/.test(token)) {
      return `<span class="token-number">${escapeHtml(token)}</span>`;
    }
    if (/^(==|=>|<=|>=|&&|\|\||\+|-|\*|\/|=)$/.test(token)) {
      return `<span class="token-operator">${escapeHtml(token)}</span>`;
    }
    if (/^[A-Za-z_]\w*$/.test(token)) {
      if (/^(function|return|const|let|var|if|else|for|while|switch|case|break|continue|class|new|try|catch|finally|throw|async|await|import|export|from|default|public|private|protected|static|void|int|float|double|bool|char|string|struct|include|namespace|using|auto|template|typename)$/.test(token)) {
        return `<span class="token-keyword">${escapeHtml(token)}</span>`;
      }
      return `<span class="token-function">${escapeHtml(token)}</span>`;
    }
    return escapeHtml(token);
  }

  function escapeHtml(text) {
    return String(text)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;")
      .replace(/'/g, "&#39;");
  }

  function escapeAttribute(text) {
    return escapeHtml(text).replace(/\n/g, "&#10;");
  }

  function copyText(text) {
    if (navigator.clipboard && window.isSecureContext) {
      return navigator.clipboard.writeText(text);
    }
    return fallbackCopyText(text);
  }

  function fallbackCopyText(text) {
    return new Promise((resolve, reject) => {
      const textarea = document.createElement("textarea");
      textarea.value = text;
      textarea.setAttribute("readonly", "readonly");
      textarea.style.position = "fixed";
      textarea.style.top = "-9999px";
      textarea.style.left = "-9999px";
      document.body.appendChild(textarea);
      textarea.focus();
      textarea.select();

      try {
        const success = document.execCommand("copy");
        document.body.removeChild(textarea);
        if (success) {
          resolve();
          return;
        }
      } catch (error) {
        document.body.removeChild(textarea);
        reject(error);
        return;
      }

      reject(new Error("copy failed"));
    });
  }
})();
