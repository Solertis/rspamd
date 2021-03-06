--[[
Copyright (c) 2017, Andrew Lewis <nerf@judo.za.org>
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

-- A plugin that forces actions

local E = {}
local N = 'force_actions'

local fun = require "fun"
local rspamd_cryptobox_hash = require "rspamd_cryptobox_hash"
local rspamd_expression = require "rspamd_expression"
local rspamd_logger = require "rspamd_logger"

local function gen_cb(expr, act, pool, message)

  local function parse_atom(str)
    local atom = table.concat(fun.totable(fun.take_while(function(c)
      if string.find(', \t()><+!|&\n', c) then
        return false
      end
      return true
    end, fun.iter(str))), '')
    return atom
  end

  local function process_atom(atom, task)
    local f_ret = task:has_symbol(atom)
    if f_ret then
      return 1
    end
    return 0
  end

  local e, err = rspamd_expression.create(expr, {parse_atom, process_atom}, pool)
  if err then
    rspamd_logger.errx(rspamd_config, 'Couldnt create expression [%1]: %2', expr, err)
    return
  end

  return function(task)

    if e:process(task) == 1 then
      if message then
        task:set_pre_result(act, message)
      else
        task:set_pre_result(act)
      end
      return true
    end

  end, e:atoms()

end

local function configure_module()
  local opts = rspamd_config:get_all_opt(N)
  if not opts then
    return false
  end
  if type(opts.actions) ~= 'table' then
    return false
  end
  for action, expressions in pairs(opts.actions) do
    if type(expressions) == 'table' then
      for _, expr in ipairs(expressions) do
        local message
        if type(expr) == 'table' then
          message = expr[2]
          expr = expr[1]
        else
          message = (opts.messages or E)[expr]
        end
        if type(expr) == 'string' then
          local cb, atoms = gen_cb(expr, action, rspamd_config:get_mempool(), message)
          if cb and atoms then
            local h = rspamd_cryptobox_hash.create()
            h:update(expr)
            local name = 'FORCE_ACTION_' .. string.upper(string.sub(h:hex(), 1, 12))
            local id = rspamd_config:register_symbol({
              type = 'normal',
              name = name,
              callback = cb,
            })
            for _, a in ipairs(atoms) do
              rspamd_config:register_dependency(id, a)
            end
            rspamd_logger.infox(rspamd_config, 'Registered symbol %1 <%2> with dependencies [%3]', name, expr, table.concat(atoms, ','))
          end
        end
      end
    end
  end
end

configure_module()
