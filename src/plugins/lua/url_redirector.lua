--[[
Copyright (c) 2017, Vsevolod Stakhov <vsevolod@highsecure.ru>

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
]]--

local redis_params
local settings = {
  expire = 86400, -- 1 day by default
  timeout = 10, -- 10 seconds by default
  nested_limit = 1, -- How many redirects to follow
  --proxy = "http://example.com:3128", -- Send request through proxy
  key_prefix = 'rdr:', -- default hash name
  check_ssl = false, -- check ssl certificates
}

local rspamd_logger = require "rspamd_logger"
local rspamd_http = require "rspamd_http"
local hash = require "rspamd_cryptobox_hash"

local function cache_url(task, orig_url, url, key, param)
  local function redis_set_cb(err, data)
    if err then
      rspamd_logger.errx(task, 'got error while setting redirect keys: %s', err)
    end
    rspamd_plugins.surbl.continue_process(url, param)
  end

  local ret = rspamd_redis_make_request(task,
    redis_params, -- connect params
    key, -- hash key
    true, -- is write
    redis_set_cb, --callback
    'SETEX', -- command
    {key, tostring(settings.expire), url} -- arguments
  )
  if not ret then
    rspamd_logger.errx(task, 'cannot make redis request to cache results')
  end
end

local function resolve_url(task, orig_url, url, key, param, ntries)
  if ntries > settings.nested_limit then
    -- We cannot resolve more, stop
    rspamd_logger.infox(task, 'cannot get more requests to resolve %s, stop on %s after %s attempts',
        orig_url, url, ntries)
    cache_url(task, orig_url, url, key, param)
  end

  local function http_callback(err, code, body, headers)
    if err then
      rspamd_logger.infox(task, 'found redirect error from %s to %s, err message: %s',
            orig_url, url, err)
      cache_url(task, orig_url, url, key, param)
    else
      if code == 200 then
        rspamd_logger.infox(task, 'found redirect from %s to %s, err code 200',
          orig_url, url)
        cache_url(task, orig_url, url, key, param)
      elseif code == 301 or code == 302 then
        local loc = headers['Location']
        rspamd_logger.infox(task, 'found redirect from %s to %s, err code 200',
          orig_url, loc)
        if loc then
          resolve_url(task, orig_url, loc, key, param, ntries + 1)
        else
          cache_url(task, orig_url, url, key, param)
        end
      else
        rspamd_logger.infox(task, 'found redirect error from %s to %s, err code: %s',
            orig_url, url, code)
        cache_url(task, orig_url, url, key, param)
      end
    end
  end

  rspamd_http.request{
    url = url,
    task = task,
    timeout = settings.timeout,
    opaque_body = true,
    no_ssl_verify = not settings.check_ssl,
    callback = http_callback
  }
end

local function url_redirector_handler(task, url, param)
  local url_str = tostring(url)
  local key = settings.key_prefix .. hash.create(url_str):base32()

  local function redis_get_cb(err, data)
    if not err then
      if type(data) == 'string' then
        if data ~= 'processing' then
          -- Got cached result
          rspamd_logger.infox(task, 'found cached redirect from %s to %s',
            url, data)
          rspamd_plugins.surbl.continue_process(data, param)
          return
        end
      end
    end
    local function redis_reserve_cb(nerr, ndata)
      if nerr then
        rspamd_logger.errx(task, 'got error while setting redirect keys: %s', nerr)
      elseif ndata == 1 then
        resolve_url(task, url_str, url_str, key, param, 1)
      end
    end

    local ret = rspamd_redis_make_request(task,
      redis_params, -- connect params
      key, -- hash key
      true, -- is write
      redis_reserve_cb, --callback
      'SETNX', -- command
      {key, 'processing'} -- arguments
    )
    if not ret then
      rspamd_logger.errx(task, 'Couldnt schedule SETNX')
    end
  end
  local ret = rspamd_redis_make_request(task,
    redis_params, -- connect params
    key, -- hash key
    false, -- is write
    redis_get_cb, --callback
    'GET', -- command
    {key} -- arguments
  )
  if not ret then
    rspamd_logger.errx(task, 'cannot make redis request to check results')
  end
end

local opts =  rspamd_config:get_all_opt('url_redirector')
if opts then
  for k,v in pairs(opts) do
    settings[k] = v
  end
  redis_params = rspamd_parse_redis_server('url_redirector')
  if not redis_params then
    rspamd_logger.infox(rspamd_config, 'no servers are specified, disabling module')
  else
    if rspamd_plugins.surbl then
      rspamd_plugins.surbl.register_redirect(url_redirector_handler)
    else
      rspamd_logger.infox(rspamd_config, 'surbl module is not enabled, disabling module')
    end
  end
end
